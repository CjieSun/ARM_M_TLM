#ifndef NVIC_H
#define NVIC_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <cstdint>

using namespace sc_core;
using namespace tlm;

class NVIC : public sc_module, public tlm_fw_transport_if<>
{
public:
    // TLM sockets
    tlm_utils::simple_target_socket<NVIC> socket;      // For register access
    tlm_utils::simple_initiator_socket<NVIC> cpu_socket; // For CPU interrupt delivery
    tlm_utils::simple_target_socket<NVIC> systick_socket; // For SysTick interrupt
    tlm_utils::simple_target_socket<NVIC> irq0_socket;    // For external IRQ0

    // Constructor
    SC_HAS_PROCESS(NVIC);
    NVIC(sc_module_name name);

    // TLM-2 interface methods
    virtual void b_transport(tlm_generic_payload& trans, sc_time& delay);
    virtual tlm_sync_enum nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& delay);
    virtual bool get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data);
    virtual unsigned int transport_dbg(tlm_generic_payload& trans);

    // Interrupt handler methods
    void systick_interrupt_handler(tlm_generic_payload& trans, sc_time& delay);
    void irq0_interrupt_handler(tlm_generic_payload& trans, sc_time& delay);

    // Public interface for triggering exceptions
    void trigger_nmi();
    void trigger_hardfault();
    void trigger_pendsv();
    void trigger_systick();
    void trigger_irq(uint32_t irq_number);

private:
    // SysTick (Cortex-M0) registers
    uint32_t m_stk_ctrl;   // 0xE000E010
    uint32_t m_stk_load;   // 0xE000E014 (24-bit)
    uint32_t m_stk_val;    // 0xE000E018 (24-bit)
    uint32_t m_stk_calib;  // 0xE000E01C (RO)
    // NVIC Registers (ARM Cortex-M0)
    uint32_t m_iser;        // 0xE000E100 - Interrupt Set Enable Register
    uint32_t m_icer;        // 0xE000E180 - Interrupt Clear Enable Register  
    uint32_t m_ispr;        // 0xE000E200 - Interrupt Set Pending Register
    uint32_t m_icpr;        // 0xE000E280 - Interrupt Clear Pending Register
    uint32_t m_ipr[8];      // 0xE000E400-0xE000E41C - Interrupt Priority Registers (8 regs for 32 IRQs)
    
    // System Handler Control and State Register
    uint32_t m_shcsr;       // 0xE000ED24 - System Handler Control and State Register
    
    // System Handler Priority Registers  
    uint32_t m_shpr2;       // 0xE000ED1C - System Handler Priority Register 2 (SVCall)
    uint32_t m_shpr3;       // 0xE000ED20 - System Handler Priority Register 3 (SysTick, PendSV)
    
    // Internal state
    uint32_t m_pending_exceptions;  // Bitmask of pending exceptions
    uint32_t m_active_exceptions;   // Bitmask of active exceptions
    
    // SysTick ticking thread
    void systick_thread();

    // Helper methods
    void handle_read(tlm_generic_payload& trans);
    void handle_write(tlm_generic_payload& trans);
    void update_interrupt_state();
    void send_exception_to_cpu(uint32_t exception_type);
    uint32_t get_highest_priority_pending_exception();
    
    // Register address mapping
    enum NVICRegister {
        NVIC_ISER   = 0xE000E100,
        NVIC_ICER   = 0xE000E180,
        NVIC_ISPR   = 0xE000E200,
        NVIC_ICPR   = 0xE000E280,
        NVIC_IPR0   = 0xE000E400,
        NVIC_IPR7   = 0xE000E41C,
    // SysTick registers
    NVIC_STK_CTRL  = 0xE000E010,
    NVIC_STK_LOAD  = 0xE000E014,
    NVIC_STK_VAL   = 0xE000E018,
    NVIC_STK_CALIB = 0xE000E01C,
        NVIC_SHCSR  = 0xE000ED24,
        NVIC_SHPR2  = 0xE000ED1C,
        NVIC_SHPR3  = 0xE000ED20
    };
};

#endif // NVIC_H