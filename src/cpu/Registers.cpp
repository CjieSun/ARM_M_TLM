#include "Registers.h"
#include "Performance.h"
#include "Log.h"

Registers::Registers(sc_module_name name) : 
    sc_module(name),
    //m_sp(0x20001000),    // Kept for backward compatibility
    m_lr(0x00000000),
    m_pc(0x00000000),
    m_psr(0x01000000),   // Default xPSR with Thumb bit set (bit 24)
    m_primask(0x00000000), // Interrupts enabled by default
    m_basepri(0x00000000), // No base priority masking
    m_faultmask(0x00000000), // Fault interrupts enabled
    m_control(0x00000000), // Privileged mode, MSP selected
    m_msp(0x20001000),   // Main Stack Pointer - default to top of RAM
    m_psp(0x00000000),    // Process Stack Pointer - initialized to 0
    m_it_firstcond(0x00), // No IT block active initially
    m_it_mask(0x00),       // No IT block active initially
    m_it_len(0x00),       // No IT block active initially
    m_it_index(0x00)      // No IT block active initially
{
    // Initialize general purpose registers to 0
    for (int i = 0; i < 13; i++) {
        m_gpr[i] = 0;
    }
    
    LOG_INFO("Registers initialized");
}

uint32_t Registers::read_register(uint8_t reg_num)
{
    Performance::getInstance().increment_register_reads();
    
    uint32_t value = 0;
    
    if (reg_num < 13) {
        value = m_gpr[reg_num];
    } else if (reg_num == 13) {
        value = get_current_sp();  // Use current SP based on CONTROL.SPSEL
    } else if (reg_num == 14) {
        value = m_lr;
    } else if (reg_num == 15) {
        value = m_pc;
    } else {
        LOG_WARNING("Invalid register number: " + std::to_string(reg_num));
        return 0;
    }
    
    if (Log::getInstance().get_log_level() >= LOG_TRACE) {
        Log::getInstance().log_register_access("R" + std::to_string(reg_num), value, false);
    }
    
    return value;
}

void Registers::write_register(uint8_t reg_num, uint32_t value)
{
    Performance::getInstance().increment_register_writes();
    
    if (reg_num < 13) {
        m_gpr[reg_num] = value;
    } else if (reg_num == 13) {
        set_current_sp(value);  // Set current SP based on CONTROL.SPSEL
    } else if (reg_num == 14) {
        m_lr = value;
    } else if (reg_num == 15) {
        m_pc = value;
    } else {
        LOG_WARNING("Invalid register number: " + std::to_string(reg_num));
        return;
    }
    
    if (Log::getInstance().get_log_level() >= LOG_TRACE) {
        Log::getInstance().log_register_access("R" + std::to_string(reg_num), value, true);
    }
}

void Registers::reset()
{
    // Reset all registers to initial values
    for (int i = 0; i < 13; i++) {
        m_gpr[i] = 0;
    }
    
    //m_sp = 0x20001000;     // Kept for backward compatibility
    m_lr = 0x00000000;
    m_pc = 0x00000000;
    m_psr = 0x01000000;    // Default xPSR with Thumb bit set (bit 24)
    
    // Reset special registers
    m_primask = 0x00000000; // Interrupts enabled
    m_basepri = 0x00000000; // No base priority masking
    m_faultmask = 0x00000000; // Fault interrupts enabled
    m_control = 0x00000000; // Privileged mode, MSP selected
    m_msp = 0x20001000;     // Main Stack Pointer
    m_psp = 0x00000000;     // Process Stack Pointer
    m_it_firstcond = 0x00;      // No IT block active
    m_it_mask = 0x00;      // No IT block active
    m_it_len = 0x00;      // No IT block active
    m_it_index = 0x00;      // No IT block active

    LOG_INFO("Registers reset");
}