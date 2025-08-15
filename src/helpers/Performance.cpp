#include "Performance.h"
#include <iostream>
#include <iomanip>

Performance& Performance::getInstance()
{
    static Performance instance;
    return instance;
}

void Performance::increment_counter(const std::string& name)
{
    m_custom_counters[name]++;
}

void Performance::start_timing()
{
    m_start_time = sc_time_stamp();
}

void Performance::stop_timing()
{
    m_total_time = sc_time_stamp() - m_start_time;
}

double Performance::get_instructions_per_second() const
{
    if (m_total_time == SC_ZERO_TIME) {
        return 0.0;
    }
    
    double seconds = m_total_time.to_seconds();
    if (seconds == 0.0) {
        return 0.0;
    }
    
    return static_cast<double>(m_instructions_executed) / seconds;
}

void Performance::print_performance_report() const
{
    std::cout << "\n=== Performance Report ===" << std::endl;
    std::cout << "Instructions executed: " << m_instructions_executed << std::endl;
    std::cout << "Memory reads: " << m_memory_reads << std::endl;
    std::cout << "Memory writes: " << m_memory_writes << std::endl;
    std::cout << "Register reads: " << m_register_reads << std::endl;
    std::cout << "Register writes: " << m_register_writes << std::endl;
    std::cout << "Branches taken: " << m_branches_taken << std::endl;
    std::cout << "IRQ count: " << m_irq_count << std::endl;
    
    if (m_total_time != SC_ZERO_TIME) {
        std::cout << "Simulation time: " << m_total_time.to_string() << std::endl;
        std::cout << "Instructions per second: " << std::fixed << std::setprecision(0) 
                  << get_instructions_per_second() << std::endl;
    }
    
    if (!m_custom_counters.empty()) {
        std::cout << "\nCustom counters:" << std::endl;
        for (const auto& counter : m_custom_counters) {
            std::cout << counter.first << ": " << counter.second << std::endl;
        }
    }
    
    std::cout << "=========================" << std::endl;
}

void Performance::reset_counters()
{
    m_instructions_executed = 0;
    m_memory_reads = 0;
    m_memory_writes = 0;
    m_register_reads = 0;
    m_register_writes = 0;
    m_branches_taken = 0;
    m_irq_count = 0;
    m_custom_counters.clear();
    m_start_time = SC_ZERO_TIME;
    m_total_time = SC_ZERO_TIME;
}