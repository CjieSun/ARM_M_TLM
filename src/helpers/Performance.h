#ifndef PERFORMANCE_H
#define PERFORMANCE_H

#include <systemc>
#include <cstdint>
#include <map>
#include <string>

using namespace sc_core;

class Performance
{
public:
    // Singleton access
    static Performance& getInstance();
    
    // Performance counters
    void increment_instructions_executed() { m_instructions_executed++; }
    void increment_memory_reads() { m_memory_reads++; }
    void increment_memory_writes() { m_memory_writes++; }
    void increment_register_reads() { m_register_reads++; }
    void increment_register_writes() { m_register_writes++; }
    void increment_branches_taken() { m_branches_taken++; }
    void increment_irq_count() { m_irq_count++; }
    
    // Add custom counter
    void increment_counter(const std::string& name);
    
    // Timing
    void start_timing();
    void stop_timing();
    
    // Statistics
    double get_instructions_per_second() const;
    uint64_t get_instructions_executed() const { return m_instructions_executed; }
    uint64_t get_memory_reads() const { return m_memory_reads; }
    uint64_t get_memory_writes() const { return m_memory_writes; }
    uint64_t get_register_reads() const { return m_register_reads; }
    uint64_t get_register_writes() const { return m_register_writes; }
    uint64_t get_branches_taken() const { return m_branches_taken; }
    uint64_t get_irq_count() const { return m_irq_count; }
    
    // Report
    void print_performance_report() const;
    void reset_counters();

private:
    // Singleton pattern
    Performance() : m_instructions_executed(0), m_memory_reads(0), m_memory_writes(0),
                   m_register_reads(0), m_register_writes(0), m_branches_taken(0),
                   m_irq_count(0), m_start_time(SC_ZERO_TIME), m_total_time(SC_ZERO_TIME) {}
    
    Performance(const Performance&) = delete;
    Performance& operator=(const Performance&) = delete;
    
    // Counters
    uint64_t m_instructions_executed;
    uint64_t m_memory_reads;
    uint64_t m_memory_writes;
    uint64_t m_register_reads;
    uint64_t m_register_writes;
    uint64_t m_branches_taken;
    uint64_t m_irq_count;
    
    // Custom counters
    std::map<std::string, uint64_t> m_custom_counters;
    
    // Timing
    sc_time m_start_time;
    sc_time m_total_time;
};

#endif // PERFORMANCE_H