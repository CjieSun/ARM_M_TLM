#ifndef REGISTERS_H
#define REGISTERS_H

#include <systemc>
#include <cstdint>

using namespace sc_core;

class Registers : public sc_module
{
public:
    // Constructor
    SC_HAS_PROCESS(Registers);
    Registers(sc_module_name name);
    
    // Register access methods
    uint32_t read_register(uint8_t reg_num);
    void write_register(uint8_t reg_num, uint32_t value);
    
    // Special register access
    uint32_t get_pc() const { return m_pc; }
    void set_pc(uint32_t pc) { m_pc = pc; }
    
    uint32_t get_psr() const { return m_psr; }
    void set_psr(uint32_t psr) { m_psr = psr; }
    
    uint32_t get_sp() const { return get_current_sp(); }
    void set_sp(uint32_t sp) { set_current_sp(sp); }
    
    uint32_t get_lr() const { return m_lr; }
    void set_lr(uint32_t lr) { m_lr = lr; }
    
    // Special register access for ARM Cortex-M
    uint32_t get_primask() const { return m_primask; }
    void set_primask(uint32_t primask) { m_primask = primask & 0x1; }  // Only bit 0 is valid
    
    uint32_t get_control() const { return m_control; }
    void set_control(uint32_t control) { m_control = control & 0x3; }  // Only bits 1-0 are valid
    
    // Stack pointer management (MSP/PSP based on CONTROL.SPSEL)
    uint32_t get_msp() const { return m_msp; }
    void set_msp(uint32_t msp) { m_msp = msp; }
    
    uint32_t get_psp() const { return m_psp; }
    void set_psp(uint32_t psp) { m_psp = psp; }
    
    // Current stack pointer based on CONTROL.SPSEL
    uint32_t get_current_sp() const { return (m_control & 0x2) ? m_psp : m_msp; }
    void set_current_sp(uint32_t sp) {
        if (m_control & 0x2) {
            m_psp = sp;
        } else {
            m_msp = sp;
        }
    }
    
    // Interrupt masking state
    bool interrupts_enabled() const { return (m_primask & 0x1) == 0; }
    void enable_interrupts() { m_primask &= ~0x1; }
    void disable_interrupts() { m_primask |= 0x1; }
    
    // Privilege level
    bool is_privileged() const { return (m_control & 0x1) == 0; }
    void set_privileged(bool privileged) { 
        if (privileged) {
            m_control &= ~0x1;
        } else {
            m_control |= 0x1;
        }
    }
    
    // PSR flag access (APSR - Application Program Status Register)
    bool get_n_flag() const { return (m_psr >> 31) & 1; }
    bool get_z_flag() const { return (m_psr >> 30) & 1; }
    bool get_c_flag() const { return (m_psr >> 29) & 1; }
    bool get_v_flag() const { return (m_psr >> 28) & 1; }
    bool get_q_flag() const { return (m_psr >> 27) & 1; }  // Saturation flag
    
    void set_n_flag(bool flag) { 
        m_psr = (m_psr & ~(1U << 31)) | (flag ? (1U << 31) : 0); 
    }
    void set_z_flag(bool flag) { 
        m_psr = (m_psr & ~(1U << 30)) | (flag ? (1U << 30) : 0); 
    }
    void set_c_flag(bool flag) { 
        m_psr = (m_psr & ~(1U << 29)) | (flag ? (1U << 29) : 0); 
    }
    void set_v_flag(bool flag) { 
        m_psr = (m_psr & ~(1U << 28)) | (flag ? (1U << 28) : 0); 
    }
    void set_q_flag(bool flag) { 
        m_psr = (m_psr & ~(1U << 27)) | (flag ? (1U << 27) : 0); 
    }
    
    // IPSR access (Interrupt Program Status Register) - bits 8-0
    uint32_t get_ipsr() const { return m_psr & 0x1FF; }
    void set_ipsr(uint32_t exception_num) { 
        m_psr = (m_psr & ~0x1FF) | (exception_num & 0x1FF);
    }
    
    // EPSR access (Execution Program Status Register) - bit 24 (Thumb)
    bool get_thumb_bit() const { return (m_psr >> 24) & 1; }
    void set_thumb_bit(bool thumb) { 
        m_psr = (m_psr & ~(1U << 24)) | (thumb ? (1U << 24) : 0); 
    }
    
    // Combined xPSR access methods
    uint32_t get_apsr() const { return m_psr & 0xF8000000; }  // N, Z, C, V, Q flags
    uint32_t get_epsr() const { return m_psr & 0x01000000; }  // Thumb bit
    void set_apsr(uint32_t apsr) { 
        m_psr = (m_psr & ~0xF8000000) | (apsr & 0xF8000000);
    }
    void set_epsr(uint32_t epsr) { 
        m_psr = (m_psr & ~0x01000000) | (epsr & 0x01000000);
    }
    
    // Exception handling helpers
    bool is_in_exception() const { return get_ipsr() != 0; }
    void enter_exception(uint32_t exception_num) { set_ipsr(exception_num); }
    void exit_exception() { set_ipsr(0); }
    
    // Reset
    void reset();

private:
    // ARM Cortex-M0 registers (R0-R12)
    uint32_t m_gpr[13];  // General Purpose Registers R0-R12
    //uint32_t m_sp;       // Stack Pointer (R13) - deprecated, use m_msp/m_psp
    uint32_t m_lr;       // Link Register (R14)  
    uint32_t m_pc;       // Program Counter (R15)
    uint32_t m_psr;      // Program Status Register (xPSR)
    
    // ARM Cortex-M special registers
    uint32_t m_primask;  // PRIMASK - interrupt mask
    uint32_t m_control;  // CONTROL - privilege level and stack selection
    uint32_t m_msp;      // Main Stack Pointer
    uint32_t m_psp;      // Process Stack Pointer
};

#endif // REGISTERS_H