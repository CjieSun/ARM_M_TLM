#ifndef TRACE_H
#define TRACE_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <fstream>
#include <iostream>

using namespace sc_core;
using namespace tlm;

class Trace : public sc_module, public tlm_fw_transport_if<>
{
public:
    // TLM target socket
    tlm_utils::simple_target_socket<Trace> socket;

    // Constructor
    SC_HAS_PROCESS(Trace);
    Trace(sc_module_name name);
    
    // Destructor
    ~Trace();

    // TLM-2 interface methods
    virtual void b_transport(tlm_generic_payload& trans, sc_time& delay);
    virtual tlm_sync_enum nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& delay);
    virtual bool get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data);
    virtual unsigned int transport_dbg(tlm_generic_payload& trans);

private:
    std::ofstream m_output_file;
    bool m_use_xterm;
    
    // Helper methods
    void write_character(char c);
    void setup_xterm_output();
};

#endif // TRACE_H