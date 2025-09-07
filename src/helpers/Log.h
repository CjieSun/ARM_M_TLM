#ifndef LOG_H
#define LOG_H

#include <systemc>
#include <fstream>
#include <string>
#include <sstream>

using namespace sc_core;

enum LogLevel {
    LOG_ERROR = 0,
    LOG_WARNING = 1,
    LOG_INFO = 2,
    LOG_DEBUG = 3,
    LOG_TRACE = 4
};

class Log
{
public:
    // Singleton access
    static Log& getInstance();
    
    // Configuration
    void set_log_level(LogLevel level) { m_log_level = level; }
    LogLevel get_log_level() const { return m_log_level; }
    void set_log_file(const std::string& filename);
    void enable_console_output(bool enable) { m_console_output = enable; }
    
    // Logging methods
    void error(const std::string& message);
    void warning(const std::string& message);
    void info(const std::string& message);
    void debug(const std::string& message);
    void trace(const std::string& message);
    
    // Instruction logging
    void log_instruction(uint32_t pc, uint16_t instruction, 
                        const std::string& name, const std::string& details);
    
    // Register logging
    void log_register_access(const std::string& reg_name, uint32_t value, bool write);
    
    // Memory logging
    void log_memory_access(uint32_t address, uint32_t value, uint32_t size, bool write);
    
    // Generic logging with level
    void log(LogLevel level, const std::string& message);
    
    // Close log file
    void close();

    std::string hex32(uint32_t v);

private:
    // Singleton pattern
    Log() : m_log_level(LOG_INFO), m_console_output(true), m_file_open(false) {}
    ~Log() { close(); }
    
    Log(const Log&) = delete;
    Log& operator=(const Log&) = delete;
    
    LogLevel m_log_level;
    std::ofstream m_log_file;
    bool m_console_output;
    bool m_file_open;
    
    // Helper methods
    std::string get_timestamp() const;
    std::string level_to_string(LogLevel level) const;
    void write_to_outputs(const std::string& message);
};

// Convenience macros
#define LOG_ERROR(msg) Log::getInstance().error(msg)
#define LOG_WARNING(msg) Log::getInstance().warning(msg)
#define LOG_INFO(msg) Log::getInstance().info(msg)
#define LOG_DEBUG(msg) Log::getInstance().debug(msg)
#define LOG_TRACE(msg) Log::getInstance().trace(msg)

#endif // LOG_H