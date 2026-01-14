#include "InteractiveUART.h"
#include "Log.h"
#include <cstdio>
#include <iostream>
#include <termios.h>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <sys/wait.h>
#include <sys/select.h>
#include <chrono>

InteractiveUART::InteractiveUART(sc_module_name name, int uart_id) :
    sc_module(name),
    socket("socket"),
    irq_socket("irq_socket"),
    m_uart_id(uart_id),
    m_pt_slave(-1),
    m_pt_master(-1),
    m_xterm_pid(-1),
    m_stop_input_thread(false)
{
    // Register TLM transport methods
    socket.register_b_transport(this, &InteractiveUART::b_transport);
    socket.register_get_direct_mem_ptr(this, &InteractiveUART::get_direct_mem_ptr);
    socket.register_transport_dbg(this, &InteractiveUART::transport_dbg);
    
    // Initialize registers
    reset_registers();
    
    // Setup xterm for I/O
    xterm_setup();
    
    // Start input monitoring thread
    m_input_thread = std::thread(&InteractiveUART::input_monitor_thread, this);
    
    // Register SystemC process
    SC_THREAD(uart_process);
    
    LOG_INFO("InteractiveUART" + std::to_string(m_uart_id) + " initialized with xterm support");
}

InteractiveUART::~InteractiveUART()
{
    // Stop input monitoring thread
    m_stop_input_thread = true;
    if (m_input_thread.joinable()) {
        m_input_thread.join();
    }
    
    // Clean up xterm
    xterm_kill();
    
    LOG_INFO("InteractiveUART" + std::to_string(m_uart_id) + " destroyed");
}

void InteractiveUART::b_transport(tlm_generic_payload& trans, sc_time& delay)
{
    uint32_t address = trans.get_address();
    uint32_t* data_ptr = reinterpret_cast<uint32_t*>(trans.get_data_ptr());
    
    if (trans.is_write()) {
        write_register(address, *data_ptr);
        trans.set_response_status(TLM_OK_RESPONSE);
    } else if (trans.is_read()) {
        *data_ptr = read_register(address);
        trans.set_response_status(TLM_OK_RESPONSE);
    } else {
        trans.set_response_status(TLM_COMMAND_ERROR_RESPONSE);
    }
    
    // Simulate hardware access delay
    delay += sc_time(50, SC_NS);
}

bool InteractiveUART::get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data)
{
    // No direct memory interface for UART peripheral
    return false;
}

unsigned int InteractiveUART::transport_dbg(tlm_generic_payload& trans)
{
    uint32_t address = trans.get_address();
    uint32_t* data_ptr = reinterpret_cast<uint32_t*>(trans.get_data_ptr());
    
    if (trans.is_write()) {
        write_register(address, *data_ptr);
    } else if (trans.is_read()) {
        *data_ptr = read_register(address);
    }
    
    return trans.get_data_length();
}

uint32_t InteractiveUART::read_register(uint32_t address)
{
    uint32_t reg_index = address / 4;
    
    if (reg_index >= 16) {
        LOG_WARNING("UART" + std::to_string(m_uart_id) + " invalid read address: 0x" + 
                   std::to_string(address));
        return 0;
    }
    
    uint32_t value = 0;
    
    switch (address) {
        case UART_RDR:
            // Read received data
            {
                std::lock_guard<std::mutex> lock(m_rx_mutex);
                if (!m_rx_buffer.empty()) {
                    value = m_rx_buffer.front();
                    m_rx_buffer.pop();
                    
                    // Clear RXNE flag if buffer is empty
                    if (m_rx_buffer.empty()) {
                        m_registers[UART_ISR / 4] &= ~ISR_RXNE;
                    }
                    
                    LOG_DEBUG("UART" + std::to_string(m_uart_id) + " RX: '" + 
                             char(value) + "' (0x" + std::to_string(value) + ")");
                } else {
                    value = 0;
                }
            }
            break;
            
        case UART_ISR:
            // Update status flags before reading
            update_status_flags();
            value = m_registers[reg_index];
            break;
            
        default:
            value = m_registers[reg_index];
            break;
    }
    
    LOG_DEBUG("UART" + std::to_string(m_uart_id) + " read: 0x" + 
             std::to_string(address) + " = 0x" + std::to_string(value));
    
    return value;
}

void InteractiveUART::write_register(uint32_t address, uint32_t value)
{
    uint32_t reg_index = address / 4;
    
    if (reg_index >= 16) {
        LOG_WARNING("UART" + std::to_string(m_uart_id) + " invalid write address: 0x" + 
                   std::to_string(address));
        return;
    }
    
    LOG_DEBUG("UART" + std::to_string(m_uart_id) + " write: 0x" + 
             std::to_string(address) + " = 0x" + std::to_string(value));
    
    switch (address) {
        case UART_TDR:
            // Transmit data
            if (m_registers[UART_CR1 / 4] & CR1_TE) {  // Transmitter enabled
                transmit_character(value & 0xFF);
                
                // Set TC (Transmission Complete) flag
                m_registers[UART_ISR / 4] |= ISR_TC;
                
                // Keep TXE (Transmit Data Register Empty) flag set
                m_registers[UART_ISR / 4] |= ISR_TXE;
            }
            break;
            
        case UART_ICR:
            // Interrupt flag clear register
            m_registers[UART_ISR / 4] &= ~value;  // Clear specified flags
            break;
            
        case UART_CR1:
        case UART_CR2:
        case UART_CR3:
        case UART_BRR:
        case UART_GTPR:
        case UART_RTOR:
        case UART_RQR:
            // Normal register write
            m_registers[reg_index] = value;
            break;
            
        default:
            // Other registers (some may be read-only)
            if (address != UART_ISR && address != UART_RDR) {
                m_registers[reg_index] = value;
            }
            break;
    }
    
    // Check for interrupt conditions after register write
    check_and_trigger_interrupts();
}

void InteractiveUART::reset_registers()
{
    // Clear all registers
    for (int i = 0; i < 16; i++) {
        m_registers[i] = 0;
    }
    
    // Set default values
    m_registers[UART_ISR / 4] = ISR_TXE | ISR_TC;  // TX empty and complete by default
}

void InteractiveUART::transmit_character(uint8_t ch)
{
    if (m_pt_slave != -1) {
        ssize_t result = write(m_pt_slave, &ch, 1);
        (void)result; // Suppress unused variable warning
        
        LOG_TRACE("UART" + std::to_string(m_uart_id) + " TX: '" + 
                 char(ch) + "' (0x" + std::to_string((int)ch) + ")");
    } else {
        // Fallback to console output
        std::cout << "UART" << m_uart_id << " TX: '" << char(ch) << "'" << std::endl;
    }
}

void InteractiveUART::receive_character(uint8_t ch)
{
    std::lock_guard<std::mutex> lock(m_rx_mutex);
    
    // Add character to receive buffer
    m_rx_buffer.push(ch);
    
    // Set RXNE (Receive Data Register Not Empty) flag
    m_registers[UART_ISR / 4] |= ISR_RXNE;
    
    // Check for buffer overrun
    if (m_rx_buffer.size() > 16) {  // Arbitrary buffer limit
        m_registers[UART_ISR / 4] |= ISR_ORE;  // Set overrun error
    }
    
    LOG_TRACE("UART" + std::to_string(m_uart_id) + " RX: '" + 
             char(ch) + "' (0x" + std::to_string((int)ch) + ")");
}

void InteractiveUART::update_status_flags()
{
    // Update RXNE flag based on buffer state
    {
        std::lock_guard<std::mutex> lock(m_rx_mutex);
        if (m_rx_buffer.empty()) {
            m_registers[UART_ISR / 4] &= ~ISR_RXNE;
        } else {
            m_registers[UART_ISR / 4] |= ISR_RXNE;
        }
    }
}

void InteractiveUART::check_and_trigger_interrupts()
{
    if (!(m_registers[UART_CR1 / 4] & CR1_UE)) {
        return;  // UART not enabled
    }
    
    bool interrupt_pending = false;
    uint32_t isr = m_registers[UART_ISR / 4];
    uint32_t cr1 = m_registers[UART_CR1 / 4];
    
    // Check RXNE interrupt
    if ((isr & ISR_RXNE) && (cr1 & CR1_RXNEIE)) {
        interrupt_pending = true;
    }
    
    // Check TC interrupt
    if ((isr & ISR_TC) && (cr1 & CR1_TCIE)) {
        interrupt_pending = true;
    }
    
    // Check TXE interrupt
    if ((isr & ISR_TXE) && (cr1 & CR1_TXEIE)) {
        interrupt_pending = true;
    }
    
    // Trigger interrupt if needed
    if (interrupt_pending) {
        // Send interrupt signal via TLM
        tlm_generic_payload trans;
        sc_time delay = SC_ZERO_TIME;
        uint32_t irq_data = 1;  // Interrupt asserted
        
        trans.set_command(TLM_WRITE_COMMAND);
        trans.set_address(0);  // Interrupt number or type
        trans.set_data_ptr(reinterpret_cast<unsigned char*>(&irq_data));
        trans.set_data_length(4);
        
        irq_socket->b_transport(trans, delay);
        
        LOG_DEBUG("UART" + std::to_string(m_uart_id) + " interrupt triggered");
    }
}

void InteractiveUART::xterm_setup()
{
    m_pt_master = open("/dev/ptmx", O_RDWR);
    
    if (m_pt_master != -1) {
        grantpt(m_pt_master);
        unlockpt(m_pt_master);
        
        char* pt_slave_name = ptsname(m_pt_master);
        m_pt_slave = open(pt_slave_name, O_RDWR);
        
        if (m_pt_slave != -1) {
            struct termios term_info{};
            tcgetattr(m_pt_slave, &term_info);
            
            term_info.c_lflag &= ~ECHO;    // Disable echo
            term_info.c_lflag &= ~ICANON;  // Disable canonical mode (raw input)
            tcsetattr(m_pt_slave, TCSADRAIN, &term_info);
            
            m_xterm_pid = fork();
            
            if (m_xterm_pid == 0) {
                xterm_launch(pt_slave_name);
            } else if (m_xterm_pid > 0) {
                LOG_INFO("UART" + std::to_string(m_uart_id) + " xterm launched with PID " + 
                        std::to_string(m_xterm_pid));
            } else {
                LOG_ERROR("UART" + std::to_string(m_uart_id) + " failed to fork xterm process");
            }
        } else {
            LOG_ERROR("UART" + std::to_string(m_uart_id) + " failed to open slave PTY");
        }
    } else {
        LOG_ERROR("UART" + std::to_string(m_uart_id) + " failed to open master PTY");
    }
}

void InteractiveUART::xterm_launch(char* slave_name) const
{
    char* arg;
    char* fin = &(slave_name[strlen(slave_name) - 2]);
    
    if (nullptr == strchr(fin, '/')) {
        arg = new char[2 + 1 + 1 + 20 + 1];
        sprintf(arg, "-S%c%c%d", fin[0], fin[1], m_pt_master);
    } else {
        char* slave_base = ::basename(slave_name);
        arg = new char[2 + strlen(slave_base) + 1 + 20 + 1];
        sprintf(arg, "-S%s/%d", slave_base, m_pt_master);
    }
    
    char title_arg[64];
    sprintf(title_arg, "-T");
    sprintf(title_arg + 2, "UART%d Console", m_uart_id);
    
    char* argv[4];
    argv[0] = (char*)("xterm");
    argv[1] = arg;
    argv[2] = title_arg;
    argv[3] = nullptr;
    
    execvp("xterm", argv);
}

void InteractiveUART::xterm_kill()
{
    if (m_pt_slave != -1) {
        close(m_pt_slave);
        m_pt_slave = -1;
    }
    
    if (m_pt_master != -1) {
        close(m_pt_master);
        m_pt_master = -1;
    }
    
    if (m_xterm_pid > 0) {
        kill(m_xterm_pid, SIGKILL);
        waitpid(m_xterm_pid, nullptr, 0);
        m_xterm_pid = -1;
    }
}

void InteractiveUART::input_monitor_thread()
{
    LOG_INFO("UART" + std::to_string(m_uart_id) + " input monitoring thread started");
    
    while (!m_stop_input_thread) {
        if (m_pt_master != -1) {
            fd_set read_fds;
            FD_ZERO(&read_fds);
            FD_SET(m_pt_master, &read_fds);
            
            struct timeval timeout;
            timeout.tv_sec = 0;
            timeout.tv_usec = 100000;  // 100ms timeout
            
            int select_result = select(m_pt_master + 1, &read_fds, nullptr, nullptr, &timeout);
            
            if (select_result > 0 && FD_ISSET(m_pt_master, &read_fds)) {
                char buffer[256];
                ssize_t bytes_read = read(m_pt_master, buffer, sizeof(buffer) - 1);
                
                if (bytes_read > 0) {
                    // Process each received character
                    for (ssize_t i = 0; i < bytes_read; i++) {
                        receive_character(buffer[i]);
                    }
                    
                    // Trigger interrupt check
                    check_and_trigger_interrupts();
                }
            }
        } else {
            // Sleep if PTY is not ready
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
    
    LOG_INFO("UART" + std::to_string(m_uart_id) + " input monitoring thread stopped");
}

void InteractiveUART::uart_process()
{
    while (true) {
        wait(10, SC_MS);  // Check every 10ms
        
        // Periodic status update and maintenance
        update_status_flags();
        
        // This process can be used for additional UART timing,
        // baud rate simulation, or other periodic tasks
    }
}