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
    m_timer(nullptr),
    m_nvic(nullptr)
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
    m_timer = new Timer("timer");
    m_nvic = new NVIC("nvic");
    
    LOG_INFO("All components created successfully");
}

void Simulator::connect_components()
{
    LOG_INFO("Connecting components...");
    
    // Connect CPU to bus controller
    m_cpu->inst_bus.bind(m_bus_ctrl->inst_socket);
    m_cpu->data_bus.bind(m_bus_ctrl->data_socket);
    
    // Connect bus controller to peripherals
    m_bus_ctrl->memory_socket.bind(m_memory->socket);
    m_bus_ctrl->trace_socket.bind(m_trace->socket);
    m_bus_ctrl->timer_socket.bind(m_timer->socket);
    m_bus_ctrl->nvic_socket.bind(m_nvic->socket);
    
    // Connect NVIC to CPU for exception delivery
    m_nvic->cpu_socket.bind(m_cpu->irq_line);
    
    // Connect timer interrupts to NVIC
    m_timer->irq_socket.bind(m_nvic->irq0_socket);        // Regular timer IRQ as external IRQ0
    m_timer->systick_socket.bind(m_nvic->systick_socket); // SysTick interrupt
    
    LOG_INFO("All components connected successfully");
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
    delete m_cpu;
    delete m_memory;
    delete m_bus_ctrl;
    delete m_trace;
    delete m_timer;
    delete m_nvic;
    
    m_cpu = nullptr;
    m_memory = nullptr;
    m_bus_ctrl = nullptr;
    m_trace = nullptr;
    m_timer = nullptr;
    m_nvic = nullptr;
}