#ifndef GDBSERVER_H
#define GDBSERVER_H

#include <systemc>
#include <string>
#include <vector>
#include <map>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <cstdint>

using namespace sc_core;

// Forward declarations
class CPU;
class Registers;
class Memory;

// GDB Server implementing GDB Remote Serial Protocol (RSP)
class GDBServer : public sc_module
{
public:
    // Constructor
    SC_HAS_PROCESS(GDBServer);
    GDBServer(sc_module_name name, int port = 3333);
    
    // Destructor
    ~GDBServer();
    
    // Set CPU reference for debugging
    void set_cpu(CPU* cpu) { m_cpu = cpu; }
    
    // Server control
    void start_server();
    void stop_server();
    bool is_running() const { return m_server_running; }
    bool wait_for_connection(int timeout_ms = 30000);  // Wait for GDB to connect
    
    // Debug control interface
    void notify_breakpoint();
    void notify_step_complete();
    void wait_for_continue();
    bool has_breakpoint(uint32_t address) const;
    
private:
    // Network communication
    int m_port;
    int m_server_socket;
    int m_client_socket;
    std::atomic<bool> m_server_running;
    std::atomic<bool> m_client_connected;
    std::thread m_server_thread;
    
    // Debug state
    std::atomic<bool> m_debug_mode;
    std::atomic<bool> m_single_step;
    std::atomic<bool> m_continue_requested;
    std::map<uint32_t, bool> m_breakpoints;
    std::mutex m_debug_mutex;
    std::condition_variable m_continue_cv;
    
    // CPU reference
    CPU* m_cpu;
    
    // GDB Protocol methods
    void server_thread();
    void handle_client();
    std::string receive_packet();
    void send_packet(const std::string& data);
    void send_ack();
    void send_nack();
    
    // GDB Command handlers
    void handle_command(const std::string& command);
    std::string handle_read_registers();
    std::string handle_write_registers(const std::string& data);
    std::string handle_read_memory(const std::string& addr_len);
    std::string handle_write_memory(const std::string& packet);
    std::string handle_continue();
    std::string handle_step();
    std::string handle_breakpoint(const std::string& packet);
    std::string handle_query(const std::string& query);
    std::string handle_v_command(const std::string& command);
    // Single register access
    std::string handle_read_register(const std::string& regnum_hex);
    std::string handle_write_register(const std::string& packet);
    
    // Utility methods
    std::string format_register_value(uint32_t value);
    uint32_t parse_hex(const std::string& hex);
    std::string to_hex(uint32_t value, int digits = 8);
    std::string to_hex(uint8_t value);
    uint8_t calculate_checksum(const std::string& data);
    
    // Socket utilities
    bool setup_server_socket();
    void cleanup_sockets();
};

#endif // GDBSERVER_H