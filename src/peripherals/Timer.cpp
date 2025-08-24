#include "Timer.h"
#include "Log.h"

Timer::Timer(sc_module_name name) : 
    sc_module(name),
    socket("socket"),
    irq_socket("irq_socket"),
    systick_socket("systick_socket"),
    m_mtime_low(0),
    m_mtime_high(0),
    m_mtimecmp_low(0xFFFFFFFF),
    m_mtimecmp_high(0xFFFFFFFF),
    m_irq_pending(false)
{
    // Bind socket
    socket.register_b_transport(this, &Timer::b_transport);
    socket.register_get_direct_mem_ptr(this, &Timer::get_direct_mem_ptr);
    socket.register_transport_dbg(this, &Timer::transport_dbg);
    
    // Start timer thread
    SC_THREAD(timer_thread);
    
    m_last_update = sc_time_stamp();
    
    LOG_INFO("Timer peripheral initialized");
}

void Timer::timer_thread()
{
    while (true) {
        wait(1, SC_MS); // Update every millisecond
        
        // Update time
        uint64_t current_ns = get_current_time_ns();
        set_mtime(current_ns);
        
        // Check for IRQ
        check_and_trigger_irq();
    }
}

void Timer::b_transport(tlm_generic_payload& trans, sc_time& delay)
{
    if (trans.get_command() == TLM_READ_COMMAND) {
        handle_read(trans);
    } else if (trans.get_command() == TLM_WRITE_COMMAND) {
        handle_write(trans);
    } else {
        trans.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
        return;
    }
    
    delay += sc_time(100, SC_NS); // Simulate peripheral access delay
}

tlm_sync_enum Timer::nb_transport_fw(tlm_generic_payload& trans, tlm_phase& phase, sc_time& delay)
{
    // Simple implementation - convert to blocking transport
    b_transport(trans, delay);
    return TLM_COMPLETED;
}

void Timer::handle_read(tlm_generic_payload& trans)
{
    uint32_t address = trans.get_address();
    uint32_t* data_ptr = reinterpret_cast<uint32_t*>(trans.get_data_ptr());
    
    switch (address) {
        case TIMER_MTIME_LOW:
            *data_ptr = m_mtime_low;
            break;
        case TIMER_MTIME_HIGH:
            *data_ptr = m_mtime_high;
            break;
        case TIMER_MTIMECMP_LOW:
            *data_ptr = m_mtimecmp_low;
            break;
        case TIMER_MTIMECMP_HIGH:
            *data_ptr = m_mtimecmp_high;
            break;
        default:
            trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            return;
    }
    
    trans.set_response_status(TLM_OK_RESPONSE);
}

void Timer::handle_write(tlm_generic_payload& trans)
{
    uint32_t address = trans.get_address();
    uint32_t* data_ptr = reinterpret_cast<uint32_t*>(trans.get_data_ptr());
    
    switch (address) {
        case TIMER_MTIME_LOW:
            m_mtime_low = *data_ptr;
            break;
        case TIMER_MTIME_HIGH:
            m_mtime_high = *data_ptr;
            break;
        case TIMER_MTIMECMP_LOW:
            m_mtimecmp_low = *data_ptr;
            m_irq_pending = false; // Clear IRQ on write
            break;
        case TIMER_MTIMECMP_HIGH:
            m_mtimecmp_high = *data_ptr;
            m_irq_pending = false; // Clear IRQ on write
            break;
        default:
            trans.set_response_status(TLM_ADDRESS_ERROR_RESPONSE);
            return;
    }
    
    trans.set_response_status(TLM_OK_RESPONSE);
}

uint64_t Timer::get_current_time_ns()
{
    sc_time current_time = sc_time_stamp();
    return static_cast<uint64_t>(current_time.to_double() * 1e9);
}

uint64_t Timer::get_mtime()
{
    return (static_cast<uint64_t>(m_mtime_high) << 32) | m_mtime_low;
}

void Timer::set_mtime(uint64_t time)
{
    m_mtime_low = static_cast<uint32_t>(time & 0xFFFFFFFF);
    m_mtime_high = static_cast<uint32_t>(time >> 32);
}

uint64_t Timer::get_mtimecmp()
{
    return (static_cast<uint64_t>(m_mtimecmp_high) << 32) | m_mtimecmp_low;
}

void Timer::set_mtimecmp(uint64_t time)
{
    m_mtimecmp_low = static_cast<uint32_t>(time & 0xFFFFFFFF);
    m_mtimecmp_high = static_cast<uint32_t>(time >> 32);
}

void Timer::check_and_trigger_irq()
{
    uint64_t mtime = get_mtime();
    uint64_t mtimecmp = get_mtimecmp();
    
    if (mtime >= mtimecmp && !m_irq_pending) {
        send_irq();
        m_irq_pending = true;
    }
}

void Timer::send_irq()
{
    tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    uint32_t irq_signal = 1;
    
    trans.set_command(TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&irq_signal));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
    
    irq_socket->b_transport(trans, delay);
    
    LOG_INFO("Timer IRQ sent");
}

void Timer::send_systick()
{
    tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    uint32_t systick_signal = 15; // SysTick is exception number 15
    
    trans.set_command(TLM_WRITE_COMMAND);
    trans.set_address(0);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&systick_signal));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
    
    systick_socket->b_transport(trans, delay);
    
    LOG_INFO("Timer SysTick sent");
}

bool Timer::get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data)
{
    return false; // No DMI for timer
}

unsigned int Timer::transport_dbg(tlm_generic_payload& trans)
{
    sc_time delay = SC_ZERO_TIME;
    b_transport(trans, delay);
    return trans.get_data_length();
}