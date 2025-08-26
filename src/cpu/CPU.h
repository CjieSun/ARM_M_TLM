#ifndef CPU_H
#define CPU_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "Registers.h"
#include "Instruction.h"
// Forward declare Execute to avoid circular header inclusion
class Execute;

using namespace sc_core;
using namespace tlm;

// ARM Cortex-M0 Exception Numbers
enum ExceptionType {
    EXCEPTION_RESET = 1,
    EXCEPTION_NMI = 2,
    EXCEPTION_HARD_FAULT = 3,
    EXCEPTION_SVCALL = 11,
    EXCEPTION_PENDSV = 14,
    EXCEPTION_SYSTICK = 15,
    EXCEPTION_IRQ0 = 16  // External interrupts start from 16
};

class CPU : public sc_module
{
public:
    // TLM sockets
    tlm_utils::simple_initiator_socket<CPU> inst_bus; // Instruction bus
    tlm_utils::simple_initiator_socket<CPU> data_bus; // Data bus
    tlm_utils::simple_target_socket<CPU> irq_line;    // IRQ line

    // Constructor
    SC_HAS_PROCESS(CPU);
    CPU(sc_module_name name);
    // Allow Execute to signal SVC
    void request_svc();
    // Try to perform an exception return when branching to EXC_RETURN magic values
    // Returns true if an exception return was performed and PC was updated
    bool try_exception_return(uint32_t exc_return);

    // Main CPU thread
    void cpu_thread();

    // Reset CPU with proper ARM M-series vector table initialization
    void reset_from_vector_table();

    // TLM-2 interface methods for IRQ
    void b_transport(tlm_generic_payload& trans, sc_time& delay);
    bool get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data);
    unsigned int transport_dbg(tlm_generic_payload& trans);

private:
    // Sub-modules
    Registers* m_registers;
    Instruction* m_instruction;
    Execute* m_execute;
    
    // Internal state
    bool m_irq_pending;
    uint32_t m_pc;
    bool m_nmi_pending;
    bool m_pendsv_pending;
    bool m_systick_pending;
    bool m_hardfault_pending;
    bool m_svc_pending;
    uint32_t m_pending_external_exception; // e.g., 16 + IRQ number when pending
    
    // Helper methods
    uint32_t fetch_instruction(uint32_t address);
    uint32_t read_memory_word(uint32_t address);  // Helper to read from memory
    void write_memory_word(uint32_t address, uint32_t data);  // Helper to write to memory
    void handle_irq();
    
    // Exception handling methods
    void handle_exception(ExceptionType exception_type);
    void trigger_exception(ExceptionType exception_type);
    uint32_t get_exception_vector_address(ExceptionType exception_type);
    void push_exception_stack_frame(uint32_t return_address);
    void check_pending_exceptions();
};

#endif // CPU_H