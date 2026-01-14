#include "Simulator.h"
#include <iostream>

Simulator::Simulator(sc_module_name name, const std::string& hex_file) : 
    sc_module(name),
    m_hex_file(hex_file),
    m_performance_enabled(true),
    m_cpu(nullptr),
    m_memory(nullptr),
    m_bus_ctrl(nullptr),
    m_trace(nullptr),
    m_nvic(nullptr),
    m_gdb_server(nullptr),
    m_gdb_enabled(false)
{
    LOG_INFO("Initializing ARM Cortex-M0 SystemC-TLM Simulator");
    
    initialize_components();
    connect_components();
    
    if (!hex_file.empty()) {
        if (!load_program()) {
            LOG_ERROR("Failed to load program");
        }
    }
    
    LOG_INFO("Simulator initialization complete");
}

Simulator::~Simulator()
{
    cleanup();
}

void Simulator::initialize_components()
{
    LOG_INFO("Creating simulator components...");
    
    // Create components
    m_cpu = new CPU("cpu");
    m_memory = new Memory("memory", 0x100000); // 1MB memory
    m_bus_ctrl = new BusCtrl("bus_ctrl");
    m_trace = new Trace("trace");
    m_nvic = new NVIC("nvic");
    
    LOG_INFO("All components created successfully");
}

void Simulator::connect_components()
{
    LOG_INFO("Connecting components...");
    
    // Setup the memory map first
    setup_memory_map();
    
    // Connect CPU to bus controller
    m_cpu->inst_bus.bind(m_bus_ctrl->inst_socket);
    m_cpu->data_bus.bind(m_bus_ctrl->data_socket);
    
    // Connect devices using the new get_device_socket method
    auto* memory_socket = m_bus_ctrl->get_device_socket("memory");
    if (memory_socket) {
        memory_socket->bind(m_memory->socket);
    }
    
    auto* trace_socket = m_bus_ctrl->get_device_socket("trace");
    if (trace_socket) {
        trace_socket->bind(m_trace->socket);
    }
    
    auto* nvic_socket = m_bus_ctrl->get_device_socket("nvic");
    if (nvic_socket) {
        nvic_socket->bind(m_nvic->socket);
    }
    
    // Connect NVIC to CPU for exception delivery
    m_nvic->cpu_socket.bind(m_cpu->irq_line);

    LOG_INFO("All components connected successfully");
}

void Simulator::setup_memory_map()
{
    LOG_INFO("Setting up memory map...");
    
    // Add standard devices
    m_bus_ctrl->add_memory(0x00000000, 0x40000000);     // Main memory space
    m_bus_ctrl->add_trace_peripheral(0x40000000, 0x4000);  // Trace peripheral  
    m_bus_ctrl->add_nvic(0xE000E000, 0x1000);              // ARM NVIC
    
    // Print the memory map
    m_bus_ctrl->print_memory_map();
}

bool Simulator::load_program()
{
    if (m_hex_file.empty()) {
        LOG_INFO("No program file specified");
        return true;
    }
    
    LOG_INFO("Loading program from: " + m_hex_file);
    
    if (!m_memory->load_hex_file(m_hex_file)) {
        LOG_ERROR("Failed to load HEX file: " + m_hex_file);
        return false;
    }
    
    LOG_INFO("Program loaded successfully");
    return true;
}

void Simulator::run_simulation(sc_time duration)
{
    LOG_INFO("Starting simulation...");
    
    if (m_performance_enabled) {
        Performance::getInstance().start_timing();
    }
    
    // Start GDB server if enabled and wait for connection
    if (m_gdb_enabled && m_gdb_server) {
        m_gdb_server->start_server();
        
        // Wait for GDB client to connect
        if (!m_gdb_server->wait_for_connection(30000)) { // 30 second timeout
            LOG_ERROR("GDB client did not connect within timeout");
            return;
        }
        
        m_cpu->set_debug_mode(true);
        m_cpu->set_debug_paused(true);  // Start in paused state
        LOG_INFO("GDB connected - starting simulation in debug mode");
    }
    
    if (duration == SC_ZERO_TIME) {
        // Run indefinitely
        sc_start();
    } else {
        // Run for specified duration
        sc_start(duration);
    }
    
    if (m_performance_enabled) {
        Performance::getInstance().stop_timing();
    }
    
    LOG_INFO("Simulation completed");
    print_final_report();
}

void Simulator::stop_simulation()
{
    LOG_INFO("Stopping simulation...");
    sc_stop();
}

void Simulator::print_final_report()
{
    if (m_performance_enabled) {
        Performance::getInstance().print_performance_report();
    }
    
    LOG_INFO("Final simulation report printed");
}

void Simulator::cleanup()
{
    if (m_gdb_server) {
        m_gdb_server->stop_server();
        delete m_gdb_server;
    }
    
    delete m_cpu;
    delete m_memory;
    delete m_bus_ctrl;
    delete m_trace;
    delete m_nvic;
    
    m_cpu = nullptr;
    m_memory = nullptr;
    m_bus_ctrl = nullptr;
    m_trace = nullptr;
    m_nvic = nullptr;
    m_gdb_server = nullptr;
}

void Simulator::enable_gdb_server(int port)
{
    if (m_gdb_server) {
        LOG_WARNING("GDB server already enabled on port " + std::to_string(port));
        return;
    }
    
    if (m_gdb_enabled) {
        LOG_WARNING("GDB server already configured");
        return;
    }
    
    m_gdb_server = new GDBServer("gdb_server", port);
    m_gdb_server->set_cpu(m_cpu);
    if (m_cpu) {
        m_cpu->set_gdb_server(m_gdb_server);
    }
    m_gdb_enabled = true;
    
    LOG_INFO("GDB server configured for port " + std::to_string(port));
}

void Simulator::disable_gdb_server()
{
    if (m_gdb_server) {
        m_gdb_server->stop_server();
        delete m_gdb_server;
        m_gdb_server = nullptr;
        m_gdb_enabled = false;
        
        if (m_cpu) {
            m_cpu->set_gdb_server(nullptr);
            m_cpu->set_debug_mode(false);
        }
        
        LOG_INFO("GDB server disabled");
    }
}