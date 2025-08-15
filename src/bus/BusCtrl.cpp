#include "BusCtrl.h"
#include "Log.h"

BusCtrl::BusCtrl(sc_module_name name) : 
    sc_module(name),
    inst_socket("inst_socket"),
    data_socket("data_socket"),
    memory_socket("memory_socket"),
    trace_socket("trace_socket"),
    timer_socket("timer_socket")
{
    // Bind target sockets
    inst_socket.register_b_transport(this, &BusCtrl::b_transport);
    inst_socket.register_get_direct_mem_ptr(this, &BusCtrl::get_direct_mem_ptr);
    inst_socket.register_transport_dbg(this, &BusCtrl::transport_dbg);
    
    data_socket.register_b_transport(this, &BusCtrl::b_transport);
    data_socket.register_get_direct_mem_ptr(this, &BusCtrl::get_direct_mem_ptr);
    data_socket.register_transport_dbg(this, &BusCtrl::transport_dbg);
    
    LOG_INFO("Bus Controller initialized");
}

void BusCtrl::b_transport(tlm_generic_payload& trans, sc_time& delay)
{
    uint32_t address = trans.get_address();
    AddressSpace space = decode_address(address);
    
    route_transaction(trans, delay, space);
}

tlm_sync_enum BusCtrl::nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& delay)
{
    // Simple implementation - convert to blocking transport
    b_transport(trans, delay);
    return TLM_COMPLETED;
}

BusCtrl::AddressSpace BusCtrl::decode_address(uint32_t address)
{
    if (address >= MEMORY_BASE && address < MEMORY_BASE + MEMORY_SIZE) {
        return ADDR_MEMORY;
    } else if (address >= TRACE_BASE && address < TRACE_BASE + TRACE_SIZE) {
        return ADDR_TRACE;
    } else if (address >= TIMER_BASE && address < TIMER_BASE + TIMER_SIZE) {
        return ADDR_TIMER;
    }
    
    return ADDR_INVALID;
}

void BusCtrl::route_transaction(tlm_generic_payload& trans, sc_time& delay, AddressSpace space)
{
    switch (space) {
        case ADDR_MEMORY:
            memory_socket->b_transport(trans, delay);
            break;
        case ADDR_TRACE:
            // Adjust address for peripheral
            trans.set_address(trans.get_address() - TRACE_BASE);
            trace_socket->b_transport(trans, delay);
            break;
        case ADDR_TIMER:
            // Adjust address for peripheral
            trans.set_address(trans.get_address() - TIMER_BASE);
            timer_socket->b_transport(trans, delay);
            break;
        case ADDR_INVALID:
        default:
            LOG_WARNING("Invalid address access: 0x" + std::to_string(trans.get_address()));
            trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            break;
    }
}

bool BusCtrl::get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data)
{
    uint32_t address = trans.get_address();
    AddressSpace space = decode_address(address);
    
    switch (space) {
        case ADDR_MEMORY:
            return memory_socket->get_direct_mem_ptr(trans, dmi_data);
        default:
            return false;  // No DMI for peripherals
    }
}

unsigned int BusCtrl::transport_dbg(tlm_generic_payload& trans)
{
    uint32_t address = trans.get_address();
    AddressSpace space = decode_address(address);
    
    switch (space) {
        case ADDR_MEMORY:
            return memory_socket->transport_dbg(trans);
        case ADDR_TRACE:
            trans.set_address(trans.get_address() - TRACE_BASE);
            return trace_socket->transport_dbg(trans);
        case ADDR_TIMER:
            trans.set_address(trans.get_address() - TIMER_BASE);
            return timer_socket->transport_dbg(trans);
        default:
            return 0;
    }
}