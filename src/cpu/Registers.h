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
    
    uint32_t get_sp() const { return m_sp; }
    void set_sp(uint32_t sp) { m_sp = sp; }
    
    uint32_t get_lr() const { return m_lr; }
    void set_lr(uint32_t lr) { m_lr = lr; }
    
    // PSR flag access
    bool get_n_flag() const { return (m_psr >> 31) & 1; }
    bool get_z_flag() const { return (m_psr >> 30) & 1; }
    bool get_c_flag() const { return (m_psr >> 29) & 1; }
    bool get_v_flag() const { return (m_psr >> 28) & 1; }
    
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
    
    // Reset
    void reset();

private:
    // ARM Cortex-M0 registers (R0-R12)
    uint32_t m_gpr[13];  // General Purpose Registers R0-R12
    uint32_t m_sp;       // Stack Pointer (R13)
    uint32_t m_lr;       // Link Register (R14)  
    uint32_t m_pc;       // Program Counter (R15)
    uint32_t m_psr;      // Program Status Register
};

#endif // REGISTERS_H