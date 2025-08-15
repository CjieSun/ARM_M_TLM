#include <systemc>
#include "Simulator.h"
#include "Log.h"
#include <iostream>
#include <string>

using namespace sc_core;

int sc_main(int argc, char* argv[])
{
    std::cout << "ARM Cortex-M0 SystemC-TLM Simulator" << std::endl;
    std::cout << "====================================" << std::endl;
    
    // Parse command line arguments
    std::string hex_file;
    std::string log_file = "simulation.log";
    LogLevel log_level = LOG_INFO;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--hex" && i + 1 < argc) {
            hex_file = argv[++i];
        } else if (arg == "--log" && i + 1 < argc) {
            log_file = argv[++i];
        } else if (arg == "--debug") {
            log_level = LOG_DEBUG;
        } else if (arg == "--trace") {
            log_level = LOG_TRACE;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "Usage: " << argv[0] << " [options]" << std::endl;
            std::cout << "Options:" << std::endl;
            std::cout << "  --hex <file>    Load Intel HEX file" << std::endl;
            std::cout << "  --log <file>    Log file (default: simulation.log)" << std::endl;
            std::cout << "  --debug         Enable debug logging" << std::endl;
            std::cout << "  --trace         Enable trace logging" << std::endl;
            std::cout << "  --help, -h      Show this help" << std::endl;
            return 0;
        }
    }
    
    try {
        // Create simulator
        Simulator sim("simulator", hex_file);
        
        // Configure logging
        sim.set_log_level(log_level);
        sim.set_log_file(log_file);
        sim.enable_performance_monitoring(true);
        
        // Run simulation
        std::cout << "Starting simulation..." << std::endl;
        if (!hex_file.empty()) {
            std::cout << "Loading HEX file: " << hex_file << std::endl;
        }
        
        sim.run_simulation(sc_time(1000, SC_US));  // Run for 1ms
        
        std::cout << "Simulation completed." << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}