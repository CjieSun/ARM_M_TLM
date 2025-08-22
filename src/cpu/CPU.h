#ifndef CPU_H
#define CPU_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <tlm_utils/simple_target_socket.h>

#include "Registers.h"
#include "Instruction.h"
#include "Execute.h"

using namespace sc_core;
using namespace tlm;

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
    
    // Helper methods
    uint32_t fetch_instruction(uint32_t address);
    uint32_t read_memory_word(uint32_t address);  // Helper to read from memory
    void handle_irq();
};

#endif // CPU_H