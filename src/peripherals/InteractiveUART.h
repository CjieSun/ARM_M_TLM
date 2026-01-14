#ifndef INTERACTIVE_UART_H
#define INTERACTIVE_UART_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_target_socket.h>
#include <tlm_utils/simple_initiator_socket.h>
#include <queue>
#include <thread>
#include <atomic>
#include <mutex>

using namespace sc_core;
using namespace tlm;

/**
 * @brief Interactive UART peripheral with xterm I/O support
 * 
 * This UART implementation creates an xterm window for interactive I/O,
 * similar to the Trace module but with full bidirectional communication.
 * It supports both character output and input with interrupt generation.
 */
class InteractiveUART : public sc_module
{
public:
    // TLM target socket for register access
    tlm_utils::simple_target_socket<InteractiveUART> socket;
    
    // TLM initiator socket for interrupt signaling
    tlm_utils::simple_initiator_socket<InteractiveUART> irq_socket;

    // Constructor
    SC_HAS_PROCESS(InteractiveUART);
    InteractiveUART(sc_module_name name, int uart_id = 0);

    // Destructor
    ~InteractiveUART();

private:
    // UART ID for identification
    int m_uart_id;
    
    // UART Register Map (AC7805x compatible)
    enum UartRegisters {
        UART_CR1    = 0x00,    // Control register 1
        UART_CR2    = 0x04,    // Control register 2
        UART_CR3    = 0x08,    // Control register 3
        UART_BRR    = 0x0C,    // Baud rate register
        UART_GTPR   = 0x10,    // Guard time and prescaler register
        UART_RTOR   = 0x14,    // Receiver timeout register
        UART_RQR    = 0x18,    // Request register
        UART_ISR    = 0x1C,    // Interrupt and status register
        UART_ICR    = 0x20,    // Interrupt flag clear register
        UART_RDR    = 0x24,    // Receive data register
        UART_TDR    = 0x28     // Transmit data register
    };
    
    // Control Register 1 (CR1) bits
    static const uint32_t CR1_UE      = (1 << 0);   // UART enable
    static const uint32_t CR1_RE      = (1 << 2);   // Receiver enable
    static const uint32_t CR1_TE      = (1 << 3);   // Transmitter enable
    static const uint32_t CR1_RXNEIE  = (1 << 5);   // RXNE interrupt enable
    static const uint32_t CR1_TCIE    = (1 << 6);   // Transmission complete interrupt enable
    static const uint32_t CR1_TXEIE   = (1 << 7);   // TXE interrupt enable
    
    // Interrupt and Status Register (ISR) bits
    static const uint32_t ISR_PE      = (1 << 0);   // Parity error
    static const uint32_t ISR_FE      = (1 << 1);   // Framing error
    static const uint32_t ISR_NF      = (1 << 2);   // Noise detected flag
    static const uint32_t ISR_ORE     = (1 << 3);   // Overrun error
    static const uint32_t ISR_IDLE    = (1 << 4);   // Idle line detected
    static const uint32_t ISR_RXNE    = (1 << 5);   // Read data register not empty
    static const uint32_t ISR_TC      = (1 << 6);   // Transmission complete
    static const uint32_t ISR_TXE     = (1 << 7);   // Transmit data register empty
    static const uint32_t ISR_BUSY    = (1 << 16);  // Busy flag
    
    // Register storage
    uint32_t m_registers[16];  // Support registers from 0x00 to 0x3C
    
    // RX buffer for received characters
    std::queue<uint8_t> m_rx_buffer;
    std::mutex m_rx_mutex;
    
    // xterm/PTY related members (similar to Trace module)
    int m_pt_slave;
    int m_pt_master;
    int m_xterm_pid;
    
    // Input monitoring
    std::thread m_input_thread;
    std::atomic<bool> m_stop_input_thread;
    
    // TLM-2 interface methods
    virtual void b_transport(tlm_generic_payload& trans, sc_time& delay);
    virtual bool get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data);
    virtual unsigned int transport_dbg(tlm_generic_payload& trans);
    
    // Register access methods
    uint32_t read_register(uint32_t address);
    void write_register(uint32_t address, uint32_t value);
    void reset_registers();
    
    // UART functionality
    void transmit_character(uint8_t ch);
    void receive_character(uint8_t ch);
    void update_status_flags();
    void check_and_trigger_interrupts();
    
    // xterm/PTY setup (adapted from Trace module)
    void xterm_setup();
    void xterm_launch(char* slave_name) const;
    void xterm_kill();
    
    // Input monitoring thread
    void input_monitor_thread();
    
    // SystemC processes
    void uart_process();
};

#endif // INTERACTIVE_UART_H