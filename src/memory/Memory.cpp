#include "Memory.h"
#include "Log.h"
#include <fstream>
#include <sstream>
#include <cstring>

Memory::Memory(sc_module_name name, uint32_t size) : 
    sc_module(name), socket("socket"), m_size(size)
{
    // Allocate memory
    m_memory = new uint8_t[m_size];
    std::memset(m_memory, 0, m_size);
    
    // Bind socket
    socket.register_b_transport(this, &Memory::b_transport);
    socket.register_get_direct_mem_ptr(this, &Memory::get_direct_mem_ptr);
    socket.register_transport_dbg(this, &Memory::transport_dbg);
    
    LOG_INFO("Memory initialized: " + std::to_string(size) + " bytes");
}

Memory::~Memory()
{
    delete[] m_memory;
}

bool Memory::load_hex_file(const std::string& filename)
{
    std::ifstream file(filename);
    if (!file.is_open()) {
        LOG_ERROR("Cannot open HEX file: " + filename);
        return false;
    }
    
    std::string line;
    uint32_t extended_address = 0;
    
    while (std::getline(file, line)) {
        if (line.empty() || line[0] != ':') {
            continue;
        }
        
        HexRecord record;
        if (!parse_hex_line(line, record)) {
            LOG_ERROR("Invalid HEX line: " + line);
            continue;
        }
        
        switch (record.record_type) {
            case 0x00: // Data record
            {
                uint32_t address = extended_address + record.address;
                if (address + record.byte_count <= m_size) {
                    std::memcpy(&m_memory[address], record.data.data(), record.byte_count);
                    LOG_DEBUG("Loaded " + std::to_string(record.byte_count) + 
                             " bytes at 0x" + std::to_string(address));
                } else {
                    LOG_WARNING("HEX data outside memory range");
                }
                break;
            }
            case 0x01: // End of file
                LOG_INFO("HEX file loaded successfully");
                return true;
            case 0x04: // Extended linear address
                if (record.byte_count == 2) {
                    extended_address = (record.data[0] << 24) | (record.data[1] << 16);
                }
                break;
        }
    }
    
    LOG_INFO("HEX file loaded successfully");
    return true;
}

void Memory::b_transport(tlm_generic_payload& trans, sc_time& delay)
{
    if (trans.get_command() == TLM_READ_COMMAND) {
        handle_read(trans);
    } else if (trans.get_command() == TLM_WRITE_COMMAND) {
        handle_write(trans);
    } else {
        trans.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    
    // Simulate memory access delay
    delay += sc_time(10, SC_NS);
}

tlm_sync_enum Memory::nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& delay)
{
    // Simple implementation - convert to blocking transport
    b_transport(trans, delay);
    return TLM_COMPLETED;
}

void Memory::handle_read(tlm_generic_payload& trans)
{
    uint32_t address = trans.get_address();
    uint32_t length = trans.get_data_length();
    uint8_t* data_ptr = trans.get_data_ptr();
    
    if (!is_valid_address(address, length)) {
        trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    
    std::memcpy(data_ptr, &m_memory[address], length);
    trans.set_response_status(TLM_OK_RESPONSE);
}

void Memory::handle_write(tlm_generic_payload& trans)
{
    uint32_t address = trans.get_address();
    uint32_t length = trans.get_data_length();
    uint8_t* data_ptr = trans.get_data_ptr();
    
    if (!is_valid_address(address, length)) {
        trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
        return;
    }
    
    std::memcpy(&m_memory[address], data_ptr, length);
    trans.set_response_status(TLM_OK_RESPONSE);
}

bool Memory::is_valid_address(uint32_t address, uint32_t length)
{
    return (address + length) <= m_size;
}

uint32_t Memory::read_word(uint32_t address)
{
    if (address + 4 > m_size) {
        return 0;
    }
    
    return *reinterpret_cast<uint32_t*>(&m_memory[address]);
}

void Memory::write_word(uint32_t address, uint32_t data)
{
    if (address + 4 > m_size) {
        return;
    }
    
    *reinterpret_cast<uint32_t*>(&m_memory[address]) = data;
}

bool Memory::get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data)
{
    // Allow DMI for performance
    dmi_data.set_dmi_ptr(m_memory);
    dmi_data.set_start_address(0);
    dmi_data.set_end_address(m_size - 1);
    dmi_data.set_granted_access(tlm::tlm_dmi::DMI_ACCESS_READ_WRITE);
    dmi_data.set_read_latency(sc_time(10, SC_NS));
    dmi_data.set_write_latency(sc_time(10, SC_NS));
    
    return true;
}

unsigned int Memory::transport_dbg(tlm_generic_payload& trans)
{
    uint32_t address = trans.get_address();
    uint32_t length = trans.get_data_length();
    uint8_t* data_ptr = trans.get_data_ptr();
    
    if (!is_valid_address(address, length)) {
        return 0;
    }
    
    if (trans.get_command() == TLM_READ_COMMAND) {
        std::memcpy(data_ptr, &m_memory[address], length);
    } else if (trans.get_command() == TLM_WRITE_COMMAND) {
        std::memcpy(&m_memory[address], data_ptr, length);
    }
    
    return length;
}

bool Memory::parse_hex_line(const std::string& line, HexRecord& record)
{
    if (line.length() < 11 || line[0] != ':') {
        return false;
    }
    
    try {
        record.byte_count = std::stoi(line.substr(1, 2), nullptr, 16);
        record.address = std::stoi(line.substr(3, 4), nullptr, 16);
        record.record_type = std::stoi(line.substr(7, 2), nullptr, 16);
        
        record.data.clear();
        for (int i = 0; i < record.byte_count; i++) {
            int pos = 9 + i * 2;
            if (pos + 1 < line.length()) {
                uint8_t byte = std::stoi(line.substr(pos, 2), nullptr, 16);
                record.data.push_back(byte);
            }
        }
        
        int checksum_pos = 9 + record.byte_count * 2;
        if (checksum_pos + 1 < line.length()) {
            record.checksum = std::stoi(line.substr(checksum_pos, 2), nullptr, 16);
        }
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

uint8_t Memory::hex_char_to_byte(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}