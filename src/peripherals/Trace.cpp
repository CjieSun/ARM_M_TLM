#include "Trace.h"
#include "Log.h"

Trace::Trace(sc_module_name name) : 
    sc_module(name), socket("socket"), m_use_xterm(false)
{
    // Bind socket
    socket.register_b_transport(this, &Trace::b_transport);
    socket.register_get_direct_mem_ptr(this, &Trace::get_direct_mem_ptr);
    socket.register_transport_dbg(this, &Trace::transport_dbg);
    
    // Setup output
    m_output_file.open("trace_output.txt", std::ios::out);
    setup_xterm_output();
    
    LOG_INFO("Trace peripheral initialized");
}

Trace::~Trace()
{
    if (m_output_file.is_open()) {
        m_output_file.close();
    }
}

void Trace::b_transport(tlm_generic_payload& trans, sc_time& delay)
{
    if (trans.get_command() != TLM_WRITE_COMMAND) {
        trans.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    
    uint32_t length = trans.get_data_length();
    uint8_t* data_ptr = trans.get_data_ptr();
    
    for (uint32_t i = 0; i < length; i++) {
        write_character(static_cast<char>(data_ptr[i]));
    }
    
    trans.set_response_status(TLM_OK_RESPONSE);
    delay += sc_time(1, SC_US); // Simulate peripheral delay
}

tlm_sync_enum Trace::nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& delay)
{
    // Simple implementation - convert to blocking transport
    b_transport(trans, delay);
    return TLM_COMPLETED;
}

void Trace::write_character(char c)
{
    // Write to console
    std::cout << c << std::flush;
    
    // Write to file
    if (m_output_file.is_open()) {
        m_output_file << c << std::flush;
    }
}

void Trace::setup_xterm_output()
{
    // For simplicity, just use console output
    // In a real implementation, this would open an xterm window
    m_use_xterm = false;
}

bool Trace::get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data)
{
    return false; // No DMI for trace peripheral
}

unsigned int Trace::transport_dbg(tlm_generic_payload& trans)
{
    // Debug transport - just pass through to normal transport
    sc_time delay = SC_ZERO_TIME;
    b_transport(trans, delay);
    return trans.get_data_length();
}