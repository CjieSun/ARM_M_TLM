#include "Log.h"
#include <iostream>
#include <sstream>

Log& Log::getInstance()
{
    static Log instance;
    return instance;
}

void Log::set_log_file(const std::string& filename)
{
    if (m_file_open) {
        m_log_file.close();
        m_file_open = false;
    }
    
    m_log_file.open(filename, std::ios::out | std::ios::trunc);
    if (m_log_file.is_open()) {
        m_file_open = true;
    }
}

void Log::error(const std::string& message)
{
    log(LOG_ERROR, message);
}

void Log::warning(const std::string& message)
{
    log(LOG_WARNING, message);
}

void Log::info(const std::string& message)
{
    log(LOG_INFO, message);
}

void Log::debug(const std::string& message)
{
    log(LOG_DEBUG, message);
}

void Log::trace(const std::string& message)
{
    log(LOG_TRACE, message);
}

void Log::log_instruction(uint32_t pc, uint16_t instruction, 
                         const std::string& name, const std::string& details)
{
    if (m_log_level >= LOG_TRACE) {
        std::stringstream ss;
        ss << "[INST] PC:0x" << std::hex << pc 
           << " OPCODE:0x" << std::hex << instruction
           << " " << name << " " << details;
        write_to_outputs(ss.str());
    }
}

void Log::log_register_access(const std::string& reg_name, uint32_t value, bool write)
{
    if (m_log_level >= LOG_TRACE) {
        std::stringstream ss;
        ss << "[REG] " << (write ? "WRITE" : "READ") 
           << " " << reg_name << " = 0x" << std::hex << value;
        write_to_outputs(ss.str());
    }
}

void Log::log_memory_access(uint32_t address, uint32_t value, uint32_t size, bool write)
{
    if (m_log_level >= LOG_TRACE) {
        std::stringstream ss;
        ss << "[MEM] " << (write ? "WRITE" : "READ") 
           << " [0x" << std::hex << address << "] = 0x" << std::hex << value 
           << " (size: " << std::dec << size << ")";
        write_to_outputs(ss.str());
    }
}

void Log::log(LogLevel level, const std::string& message)
{
    if (level <= m_log_level) {
        std::string full_message = get_timestamp() + " [" + level_to_string(level) + "] " + message;
        write_to_outputs(full_message);
    }
}

void Log::close()
{
    if (m_file_open) {
        m_log_file.close();
        m_file_open = false;
    }
}

std::string Log::get_timestamp() const
{
    std::stringstream ss;
    ss << sc_time_stamp().to_string();
    return ss.str();
}

std::string Log::level_to_string(LogLevel level) const
{
    switch (level) {
        case LOG_ERROR:   return "ERROR";
        case LOG_WARNING: return "WARNING";
        case LOG_INFO:    return "INFO";
        case LOG_DEBUG:   return "DEBUG";
        case LOG_TRACE:   return "TRACE";
        default:          return "UNKNOWN";
    }
}

void Log::write_to_outputs(const std::string& message)
{
    if (m_console_output) {
        std::cout << message << std::endl;
    }
    
    if (m_file_open && m_log_file.is_open()) {
        m_log_file << message << std::endl;
        m_log_file.flush();
    }
}