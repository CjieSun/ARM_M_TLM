#ifndef TIMER_H
#define TIMER_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>

using namespace sc_core;
using namespace tlm;

class Timer : public sc_module, public tlm_fw_transport_if<>
{
public:
    // TLM sockets
    tlm_utils::simple_target_socket<Timer> socket;      // For register access
    tlm_utils::simple_initiator_socket<Timer> irq_socket; // For IRQ generation

    // Constructor
    SC_HAS_PROCESS(Timer);
    Timer(sc_module_name name);

    // TLM-2 interface methods
    virtual void b_transport(tlm_generic_payload& trans, sc_time& delay);
    virtual tlm_sync_enum nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& delay);
    virtual bool get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data);
    virtual unsigned int transport_dbg(tlm_generic_payload& trans);

    // Timer thread
    void timer_thread();

private:
    // Timer registers (64-bit split into 32-bit parts)
    uint32_t m_mtime_low;       // 0x40004000 - Current time (low 32 bits)
    uint32_t m_mtime_high;      // 0x40004004 - Current time (high 32 bits)
    uint32_t m_mtimecmp_low;    // 0x40004008 - Compare time (low 32 bits)
    uint32_t m_mtimecmp_high;   // 0x4000400C - Compare time (high 32 bits)
    
    // Internal state
    sc_time m_last_update;
    bool m_irq_pending;
    
    // Helper methods
    uint64_t get_current_time_ns();
    uint64_t get_mtime();
    void set_mtime(uint64_t time);
    uint64_t get_mtimecmp();
    void set_mtimecmp(uint64_t time);
    void check_and_trigger_irq();
    void send_irq();
    
    // Register access
    void handle_read(tlm_generic_payload& trans);
    void handle_write(tlm_generic_payload& trans);
    
    // Address mapping
    enum TimerRegister {
        TIMER_MTIME_LOW   = 0x0,  // Offset from base
        TIMER_MTIME_HIGH  = 0x4,
        TIMER_MTIMECMP_LOW  = 0x8,
        TIMER_MTIMECMP_HIGH = 0xC
    };
};

#endif // TIMER_H