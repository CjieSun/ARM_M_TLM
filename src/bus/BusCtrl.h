#ifndef BUSCTRL_H
#define BUSCTRL_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>

using namespace sc_core;
using namespace tlm;

class BusCtrl : public sc_module, public tlm_fw_transport_if<>
{
public:
    // Target sockets (from CPU)
    tlm_utils::simple_target_socket<BusCtrl> inst_socket; // Instruction bus
    tlm_utils::simple_target_socket<BusCtrl> data_socket; // Data bus
    
    // Initiator sockets (to peripherals)
    tlm_utils::simple_initiator_socket<BusCtrl> memory_socket; // Memory
    tlm_utils::simple_initiator_socket<BusCtrl> trace_socket;  // Trace peripheral
    tlm_utils::simple_initiator_socket<BusCtrl> timer_socket;  // Timer peripheral
    tlm_utils::simple_initiator_socket<BusCtrl> nvic_socket;   // NVIC peripheral

    // Constructor
    SC_HAS_PROCESS(BusCtrl);
    BusCtrl(sc_module_name name);

    // TLM-2 interface methods
    virtual void b_transport(tlm_generic_payload& trans, sc_time& delay);
    virtual tlm_sync_enum nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& delay);
    virtual bool get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data);
    virtual unsigned int transport_dbg(tlm_generic_payload& trans);

private:
    // Address decoding
    enum AddressSpace {
        ADDR_MEMORY,    // 0x00000000 - 0x3FFFFFFF
        ADDR_TRACE,     // 0x40000000 - 0x40003FFF  
        ADDR_TIMER,     // 0x40004000 - 0x40007FFF
        ADDR_NVIC,      // 0xE000E000 - 0xE000EFFF (ARM Cortex-M0 NVIC)
        ADDR_INVALID
    };
    
    AddressSpace decode_address(uint32_t address);
    void route_transaction(tlm_generic_payload& trans, sc_time& delay, AddressSpace space);
    
    // Memory map constants
    static const uint32_t MEMORY_BASE = 0x00000000;
    static const uint32_t MEMORY_SIZE = 0x40000000;
    static const uint32_t TRACE_BASE  = 0x40000000;
    static const uint32_t TRACE_SIZE  = 0x00004000;
    static const uint32_t TIMER_BASE  = 0x40004000;
    static const uint32_t TIMER_SIZE  = 0x00004000;
    static const uint32_t NVIC_BASE   = 0xE000E000;
    static const uint32_t NVIC_SIZE   = 0x00001000;
};

#endif // BUSCTRL_H