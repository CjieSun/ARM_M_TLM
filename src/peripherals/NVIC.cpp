#include "NVIC.h"
#include "Log.h"
#include <sstream>
#include <cstring>

NVIC::NVIC(sc_module_name name) : 
    sc_module(name),
    socket("socket"),
    cpu_socket("cpu_socket"),
    systick_socket("systick_socket"),
    m_iser(0),
    m_icer(0),
    m_ispr(0),
    m_icpr(0),
    m_shcsr(0),
    m_shpr2(0),
    m_shpr3(0),
    m_pending_exceptions(0),
    m_active_exceptions(0)
{
    // Initialize priority registers
    for (int i = 0; i < 8; i++) {
        m_ipr[i] = 0;
    }
    
    // Bind sockets
    socket.register_b_transport(this, &NVIC::b_transport);
    socket.register_get_direct_mem_ptr(this, &NVIC::get_direct_mem_ptr);
    socket.register_transport_dbg(this, &NVIC::transport_dbg);
    
    LOG_INFO("NVIC peripheral initialized");
}

void NVIC::b_transport(tlm_generic_payload& trans, sc_time& delay)
{
    if (trans.get_command() == TLM_READ_COMMAND) {
        handle_read(trans);
    } else if (trans.get_command() == TLM_WRITE_COMMAND) {
        handle_write(trans);
    } else {
        trans.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    
    delay += sc_time(10, SC_NS); // Simulate register access delay
}

void NVIC::handle_read(tlm_generic_payload& trans)
{
    uint32_t address = trans.get_address();
    uint32_t* data_ptr = reinterpret_cast<uint32_t*>(trans.get_data_ptr());
    uint32_t length = trans.get_data_length();
    
    if (length != 4) {
        trans.set_response_status(TLM_BURST_ERROR_RESPONSE);
        return;
    }
    
    uint32_t value = 0;
    
    switch (address) {
        case NVIC_ISER:
            value = m_iser;
            break;
        case NVIC_ICER:
            value = m_icer;
            break;
        case NVIC_ISPR:
            value = m_ispr;
            break;
        case NVIC_ICPR:
            value = m_icpr;
            break;
        case NVIC_SHCSR:
            value = m_shcsr;
            break;
        case NVIC_SHPR2:
            value = m_shpr2;
            break;
        case NVIC_SHPR3:
            value = m_shpr3;
            break;
        default:
            // Check if it's an IPR register
            if (address >= NVIC_IPR0 && address <= NVIC_IPR7) {
                uint32_t ipr_index = (address - NVIC_IPR0) / 4;
                if (ipr_index < 8) {
                    value = m_ipr[ipr_index];
                } else {
                    trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
                    return;
                }
            } else {
                trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
                return;
            }
            break;
    }
    
    *data_ptr = value;
    trans.set_response_status(TLM_OK_RESPONSE);
    
    LOG_DEBUG("NVIC read: 0x" + std::to_string(address) + " = 0x" + std::to_string(value));
}

void NVIC::handle_write(tlm_generic_payload& trans)
{
    uint32_t address = trans.get_address();
    uint32_t* data_ptr = reinterpret_cast<uint32_t*>(trans.get_data_ptr());
    uint32_t length = trans.get_data_length();
    
    if (length != 4) {
        trans.set_response_status(TLM_BURST_ERROR_RESPONSE);
        return;
    }
    
    uint32_t value = *data_ptr;
    
    switch (address) {
        case NVIC_ISER:
            m_iser |= value;  // Set bits
            break;
        case NVIC_ICER:
            m_iser &= ~value; // Clear bits in ISER
            m_icer = 0;       // ICER is write-only
            break;
        case NVIC_ISPR:
            m_ispr |= value;  // Set pending bits
            m_pending_exceptions |= value; // Update internal pending state
            break;
        case NVIC_ICPR:
            m_ispr &= ~value; // Clear pending bits in ISPR
            m_pending_exceptions &= ~value; // Update internal pending state
            m_icpr = 0;       // ICPR is write-only
            break;
        case NVIC_SHCSR:
            m_shcsr = value;
            break;
        case NVIC_SHPR2:
            m_shpr2 = value;
            break;
        case NVIC_SHPR3:
            m_shpr3 = value;
            break;
        default:
            // Check if it's an IPR register
            if (address >= NVIC_IPR0 && address <= NVIC_IPR7) {
                uint32_t ipr_index = (address - NVIC_IPR0) / 4;
                if (ipr_index < 8) {
                    m_ipr[ipr_index] = value;
                } else {
                    trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
                    return;
                }
            } else {
                trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
                return;
            }
            break;
    }
    
    trans.set_response_status(TLM_OK_RESPONSE);
    LOG_DEBUG("NVIC write: 0x" + std::to_string(address) + " = 0x" + std::to_string(value));
    
    // Update interrupt state after any write
    update_interrupt_state();
}

void NVIC::trigger_nmi()
{
    LOG_INFO("NVIC: Triggering NMI");
    send_exception_to_cpu(2); // NMI is exception number 2
}

void NVIC::trigger_hardfault()
{
    LOG_INFO("NVIC: Triggering HardFault");
    send_exception_to_cpu(3); // HardFault is exception number 3
}

void NVIC::trigger_pendsv()
{
    LOG_INFO("NVIC: Triggering PendSV");
    m_shcsr |= (1 << 28); // Set PENDSVSET bit
    send_exception_to_cpu(14); // PendSV is exception number 14
}

void NVIC::trigger_systick()
{
    LOG_INFO("NVIC: Triggering SysTick");
    send_exception_to_cpu(15); // SysTick is exception number 15
}

void NVIC::trigger_irq(uint32_t irq_number)
{
    if (irq_number < 32) {
        LOG_INFO("NVIC: Triggering IRQ " + std::to_string(irq_number));
        uint32_t bit = 1 << irq_number;
        m_ispr |= bit;
        m_pending_exceptions |= bit;
        update_interrupt_state();
    } else {
        LOG_WARNING("NVIC: Invalid IRQ number: " + std::to_string(irq_number));
    }
}

void NVIC::update_interrupt_state()
{
    // Check if any enabled interrupts are pending
    uint32_t enabled_and_pending = m_iser & m_ispr;
    
    if (enabled_and_pending != 0) {
        uint32_t highest_priority_irq = get_highest_priority_pending_exception();
        if (highest_priority_irq != 0) {
            // Send IRQ signal to CPU (IRQs start at exception number 16)
            send_exception_to_cpu(16 + highest_priority_irq);
        }
    }
}

uint32_t NVIC::get_highest_priority_pending_exception()
{
    uint32_t enabled_and_pending = m_iser & m_ispr;
    
    // Find the lowest numbered (highest priority) pending IRQ
    for (uint32_t i = 0; i < 32; i++) {
        if (enabled_and_pending & (1 << i)) {
            return i;
        }
    }
    
    return 0; // No pending interrupts
}

void NVIC::send_exception_to_cpu(uint32_t exception_type)
{
    tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    
    trans.set_command(TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&exception_type));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
    
    cpu_socket->b_transport(trans, delay);
    
    if (trans.get_response_status() == TLM_OK_RESPONSE) {
        LOG_DEBUG("NVIC: Exception " + std::to_string(exception_type) + " sent to CPU");
    } else {
        LOG_ERROR("NVIC: Failed to send exception to CPU");
    }
}

tlm_sync_enum NVIC::nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& delay)
{
    // Simple implementation - convert to blocking transport
    b_transport(trans, delay);
    return TLM_COMPLETED;
}

bool NVIC::get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data)
{
    return false; // No DMI for NVIC
}

unsigned int NVIC::transport_dbg(tlm_generic_payload& trans)
{
    sc_time delay = SC_ZERO_TIME;
    b_transport(trans, delay);
    return trans.get_data_length();
}