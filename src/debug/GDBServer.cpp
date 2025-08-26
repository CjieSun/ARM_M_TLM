#include "GDBServer.h"
#include "CPU.h"
#include "Registers.h"
#include "Log.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <iostream>
#include <sstream>
#include <iomanip>

GDBServer::GDBServer(sc_module_name name, int port) :
    sc_module(name),
    m_port(port),
    m_server_socket(-1),
    m_client_socket(-1),
    m_server_running(false),
    m_client_connected(false),
    m_debug_mode(false),
    m_single_step(false),
    m_continue_requested(false),
    m_cpu(nullptr)
{
    LOG_INFO("GDB Server initialized on port " + std::to_string(port));
}

GDBServer::~GDBServer()
{
    stop_server();
}

void GDBServer::start_server()
{
    if (m_server_running) {
        LOG_WARNING("GDB Server already running on port " + std::to_string(m_port));
        return;
    }
    
    if (!setup_server_socket()) {
        LOG_ERROR("Failed to setup GDB server socket on port " + std::to_string(m_port));
        return;
    }
    
    m_server_running = true;
    m_server_thread = std::thread(&GDBServer::server_thread, this);
    
    LOG_INFO("GDB Server started on port " + std::to_string(m_port));
}

void GDBServer::stop_server()
{
    if (!m_server_running) {
        return;
    }
    
    m_server_running = false;
    m_client_connected = false;
    
    // Wake up any waiting debug operations
    {
        std::lock_guard<std::mutex> lock(m_debug_mutex);
        m_continue_requested = true;
        m_continue_cv.notify_all();
    }
    
    cleanup_sockets();
    
    if (m_server_thread.joinable()) {
        m_server_thread.join();
    }
    
    LOG_INFO("GDB Server stopped");
}

bool GDBServer::setup_server_socket()
{
    m_server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_server_socket < 0) {
        LOG_ERROR("Failed to create socket");
        return false;
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(m_server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        LOG_WARNING("Failed to set SO_REUSEADDR");
    }
    
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(m_port);
    
    if (bind(m_server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        LOG_ERROR("Failed to bind to port " + std::to_string(m_port) + ". Port may be in use.");
        close(m_server_socket);
        m_server_socket = -1;
        return false;
    }
    
    if (listen(m_server_socket, 1) < 0) {
        LOG_ERROR("Failed to listen on socket");
        close(m_server_socket);
        m_server_socket = -1;
        return false;
    }
    
    return true;
}

void GDBServer::cleanup_sockets()
{
    if (m_client_socket >= 0) {
        close(m_client_socket);
        m_client_socket = -1;
    }
    
    if (m_server_socket >= 0) {
        close(m_server_socket);
        m_server_socket = -1;
    }
}

void GDBServer::server_thread()
{
    while (m_server_running) {
        LOG_INFO("GDB Server waiting for connection on port " + std::to_string(m_port));
        
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        m_client_socket = accept(m_server_socket, (struct sockaddr*)&client_addr, &client_len);
        
        if (m_client_socket < 0) {
            if (m_server_running) {
                LOG_ERROR("GDB Server accept failed");
            }
            continue;
        }
        
        m_client_connected = true;
        m_debug_mode = true;
        
        std::string client_ip = inet_ntoa(client_addr.sin_addr);
        LOG_INFO("GDB client connected from " + client_ip);
        
        try {
            handle_client();
        } catch (const std::exception& e) {
            LOG_ERROR("GDB Server error: " + std::string(e.what()));
        }
        
        close(m_client_socket);
        m_client_socket = -1;
        m_client_connected = false;
        m_debug_mode = false;
        
        LOG_INFO("GDB client disconnected");
    }
}

void GDBServer::handle_client()
{
    // Send initial stop reason
    send_packet("S05"); // SIGTRAP
    
    while (m_client_connected && m_server_running) {
        std::string packet = receive_packet();
        if (packet.empty()) {
            break;
        }
        
        LOG_DEBUG("GDB received: " + packet);
        handle_command(packet);
    }
}

std::string GDBServer::receive_packet()
{
    char buffer[4096];
    std::string packet;
    bool in_packet = false;
    
    while (m_client_connected) {
        int bytes = recv(m_client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes <= 0) {
            break;
        }
        
        buffer[bytes] = '\0';
        
        for (int i = 0; i < bytes; i++) {
            char c = buffer[i];
            
            if (c == '$') {
                packet.clear();
                in_packet = true;
            } else if (c == '#' && in_packet) {
                // Get checksum (2 hex digits)
                if (i + 2 < bytes) {
                    // Verify checksum
                    uint8_t received_checksum = (parse_hex(std::string(1, buffer[i+1])) << 4) |
                                               parse_hex(std::string(1, buffer[i+2]));
                    uint8_t calculated_checksum = calculate_checksum(packet);
                    
                    if (received_checksum == calculated_checksum) {
                        send_ack();
                        return packet;
                    } else {
                        send_nack();
                    }
                }
                in_packet = false;
                packet.clear();
                i += 2; // Skip checksum bytes
            } else if (in_packet) {
                packet += c;
            } else if (c == '+') {
                // ACK received
            } else if (c == '-') {
                // NACK received - resend last packet
            }
        }
    }
    
    return "";
}

void GDBServer::send_packet(const std::string& data)
{
    if (!m_client_connected) return;
    
    uint8_t checksum = calculate_checksum(data);
    std::string packet = "$" + data + "#" + to_hex(checksum);
    
    LOG_DEBUG("GDB sending: " + packet);
    send(m_client_socket, packet.c_str(), packet.length(), 0);
}

void GDBServer::send_ack()
{
    if (m_client_connected) {
        send(m_client_socket, "+", 1, 0);
    }
}

void GDBServer::send_nack()
{
    if (m_client_connected) {
        send(m_client_socket, "-", 1, 0);
    }
}

void GDBServer::handle_command(const std::string& command)
{
    if (command.empty()) return;
    
    char cmd = command[0];
    std::string response;
    
    switch (cmd) {
        case 'g':
            response = handle_read_registers();
            break;
        case 'G':
            response = handle_write_registers(command.substr(1));
            break;
        case 'm':
            response = handle_read_memory(command.substr(1));
            break;
        case 'M':
            response = handle_write_memory(command);
            break;
        case 'c':
            response = handle_continue();
            break;
        case 's':
            response = handle_step();
            break;
        case 'Z':
        case 'z':
            response = handle_breakpoint(command);
            break;
        case 'q':
            response = handle_query(command.substr(1));
            break;
        case '?':
            response = "S05"; // SIGTRAP
            break;
        case 'k':
            // Kill command
            m_client_connected = false;
            return;
        default:
            response = ""; // Empty response for unsupported commands
            break;
    }
    
    send_packet(response);
}

std::string GDBServer::handle_read_registers()
{
    if (!m_cpu) {
        return "E01";
    }
    
    // Get CPU registers - ARM has 16 general registers (R0-R15)
    std::string response;
    Registers* regs = m_cpu->get_registers();
    
    // R0-R12 (general purpose registers)
    for (int i = 0; i < 13; i++) {
        uint32_t value = regs->read_register(i);
        response += format_register_value(value);
    }
    
    // R13 (SP), R14 (LR), R15 (PC)
    response += format_register_value(regs->get_sp());
    response += format_register_value(regs->get_lr());
    response += format_register_value(regs->get_pc());
    
    // PSR (Program Status Register) - usually sent after general registers
    response += format_register_value(regs->get_psr());
    
    return response;
}

std::string GDBServer::handle_write_registers(const std::string& data)
{
    if (!m_cpu) {
        return "E01";
    }
    
    if (data.length() < 16 * 8) { // 16 registers * 8 hex chars per register
        return "E02";
    }
    
    Registers* regs = m_cpu->get_registers();
    
    // Parse register values from hex string
    for (int i = 0; i < 13; i++) {
        std::string hex_value = data.substr(i * 8, 8);
        uint32_t value = parse_hex(hex_value);
        regs->write_register(i, value);
    }
    
    // SP, LR, PC
    regs->set_sp(parse_hex(data.substr(13 * 8, 8)));
    regs->set_lr(parse_hex(data.substr(14 * 8, 8)));
    regs->set_pc(parse_hex(data.substr(15 * 8, 8)));
    
    // PSR
    if (data.length() >= 17 * 8) {
        regs->set_psr(parse_hex(data.substr(16 * 8, 8)));
    }
    
    return "OK";
}

std::string GDBServer::handle_read_memory(const std::string& addr_len)
{
    size_t comma_pos = addr_len.find(',');
    if (comma_pos == std::string::npos) {
        return "E01";
    }
    
    uint32_t address = parse_hex(addr_len.substr(0, comma_pos));
    uint32_t length = parse_hex(addr_len.substr(comma_pos + 1));
    
    if (length > 1024) { // Limit read size
        return "E02";
    }
    
    std::string response;
    
    // Read memory through CPU's memory interface
    for (uint32_t i = 0; i < length; i++) {
        try {
            uint32_t data = m_cpu->read_memory_debug(address + i);
            response += to_hex(static_cast<uint8_t>(data));
        } catch (...) {
            return "E03";
        }
    }
    
    return response;
}

std::string GDBServer::handle_write_memory(const std::string& packet)
{
    // Format: Maddr,length:XX...
    size_t comma_pos = packet.find(',', 1);
    size_t colon_pos = packet.find(':', comma_pos);
    
    if (comma_pos == std::string::npos || colon_pos == std::string::npos) {
        return "E01";
    }
    
    uint32_t address = parse_hex(packet.substr(1, comma_pos - 1));
    uint32_t length = parse_hex(packet.substr(comma_pos + 1, colon_pos - comma_pos - 1));
    std::string data = packet.substr(colon_pos + 1);
    
    if (data.length() != length * 2) { // Each byte is 2 hex chars
        return "E02";
    }
    
    // Write memory through CPU's memory interface
    for (uint32_t i = 0; i < length; i++) {
        try {
            uint8_t byte_value = static_cast<uint8_t>(parse_hex(data.substr(i * 2, 2)));
            m_cpu->write_memory_debug(address + i, byte_value);
        } catch (...) {
            return "E03";
        }
    }
    
    return "OK";
}

std::string GDBServer::handle_continue()
{
    {
        std::lock_guard<std::mutex> lock(m_debug_mutex);
        m_continue_requested = true;
        m_single_step = false;
        m_debug_mode = true; // Ensure we stay in debug mode
        m_continue_cv.notify_all();
    }
    
    // Don't send response immediately - will send stop reason when execution stops
    return "";
}

std::string GDBServer::handle_step()
{
    {
        std::lock_guard<std::mutex> lock(m_debug_mutex);
        m_single_step = true;
        m_continue_requested = true;
        m_debug_mode = true; // Ensure we stay in debug mode  
        m_continue_cv.notify_all();
    }
    
    // Don't send response immediately - will send stop reason when step completes
    return "";
}

std::string GDBServer::handle_breakpoint(const std::string& packet)
{
    // Format: Z0,addr,kind or z0,addr,kind
    if (packet.length() < 5) return "E01";
    
    bool insert = (packet[0] == 'Z');
    char type = packet[1];
    
    if (type != '0') return ""; // Only software breakpoints supported
    
    size_t comma1 = packet.find(',', 2);
    size_t comma2 = packet.find(',', comma1 + 1);
    
    if (comma1 == std::string::npos || comma2 == std::string::npos) {
        return "E01";
    }
    
    uint32_t address = parse_hex(packet.substr(comma1 + 1, comma2 - comma1 - 1));
    
    if (insert) {
        m_breakpoints[address] = true;
    } else {
        m_breakpoints.erase(address);
    }
    
    return "OK";
}

std::string GDBServer::handle_query(const std::string& query)
{
    if (query.substr(0, 9) == "Supported") {
        return "PacketSize=4000";
    } else if (query == "C") {
        return "QC1";
    }
    
    return "";
}

void GDBServer::notify_breakpoint()
{
    if (m_client_connected) {
        send_packet("S05"); // SIGTRAP
    }
}

void GDBServer::notify_step_complete()
{
    if (m_client_connected) {
        send_packet("S05"); // SIGTRAP
    }
}

void GDBServer::wait_for_continue()
{
    if (!m_debug_mode || !m_server_running) {
        return;
    }
    
    std::unique_lock<std::mutex> lock(m_debug_mutex);
    m_continue_requested = false; // Reset the flag
    
    // Wait until continue is requested or server stops
    m_continue_cv.wait(lock, [this] { 
        return m_continue_requested || !m_server_running || !m_debug_mode; 
    });
    
    m_continue_requested = false; // Reset after waking up
}

// Utility methods
std::string GDBServer::format_register_value(uint32_t value)
{
    // GDB expects little-endian byte order
    std::string result;
    for (int i = 0; i < 4; i++) {
        result += to_hex(static_cast<uint8_t>(value & 0xFF));
        value >>= 8;
    }
    return result;
}

uint32_t GDBServer::parse_hex(const std::string& hex)
{
    uint32_t result = 0;
    for (char c : hex) {
        result <<= 4;
        if (c >= '0' && c <= '9') {
            result |= (c - '0');
        } else if (c >= 'a' && c <= 'f') {
            result |= (c - 'a' + 10);
        } else if (c >= 'A' && c <= 'F') {
            result |= (c - 'A' + 10);
        }
    }
    return result;
}

std::string GDBServer::to_hex(uint32_t value, int digits)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(digits) << value;
    return oss.str();
}

std::string GDBServer::to_hex(uint8_t value)
{
    std::ostringstream oss;
    oss << std::hex << std::setfill('0') << std::setw(2) << static_cast<int>(value);
    return oss.str();
}

uint8_t GDBServer::calculate_checksum(const std::string& data)
{
    uint8_t checksum = 0;
    for (char c : data) {
        checksum += static_cast<uint8_t>(c);
    }
    return checksum;
}