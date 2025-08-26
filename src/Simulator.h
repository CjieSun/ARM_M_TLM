#ifndef SIMULATOR_H
#define SIMULATOR_H

#include <systemc>
#include <string>
#include "CPU.h"
#include "Memory.h"
#include "BusCtrl.h"
#include "Trace.h"
#include "NVIC.h"
#include "Performance.h"
#include "Log.h"
#include "GDBServer.h"

using namespace sc_core;

class Simulator : public sc_module
{
public:
    // Constructor
    SC_HAS_PROCESS(Simulator);
    Simulator(sc_module_name name, const std::string& hex_file = "");
    
    // Destructor
    ~Simulator();
    
    // Configuration
    void set_hex_file(const std::string& hex_file) { m_hex_file = hex_file; }
    void set_log_level(LogLevel level) { Log::getInstance().set_log_level(level); }
    void set_log_file(const std::string& log_file) { Log::getInstance().set_log_file(log_file); }
    void enable_performance_monitoring(bool enable) { m_performance_enabled = enable; }
    void enable_gdb_server(int port = 3333);
    void disable_gdb_server();
    
    // Simulation control
    void run_simulation(sc_time duration = SC_ZERO_TIME);
    void stop_simulation();
    
private:
    // Components
    CPU* m_cpu;
    Memory* m_memory;
    BusCtrl* m_bus_ctrl;
    Trace* m_trace;
    NVIC* m_nvic;
    GDBServer* m_gdb_server;
    
    // Configuration
    std::string m_hex_file;
    bool m_performance_enabled;
    bool m_gdb_enabled;
    
    // Initialization
    void initialize_components();
    void connect_components();
    bool load_program();
    
    // Cleanup
    void cleanup();
    void print_final_report();
};

#endif // SIMULATOR_H