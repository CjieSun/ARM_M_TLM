#include "CPU.h"
#include "Performance.h"
#include "Log.h"
#include <sstream>

CPU::CPU(sc_module_name name) : 
    sc_module(name),
    inst_bus("inst_bus"),
    data_bus("data_bus"),
    irq_line("irq_line"),
    m_irq_pending(false),
    m_pc(0),
    m_nmi_pending(false),
    m_pendsv_pending(false),
    m_systick_pending(false),
    m_hardfault_pending(false)
{
    // Initialize sub-modules
    m_registers = new Registers("registers");
    m_instruction = new Instruction("instruction");
    m_execute = new Execute("execute", m_registers);
    
    // Bind IRQ socket
    irq_line.register_b_transport(this, &CPU::b_transport);
    irq_line.register_get_direct_mem_ptr(this, &CPU::get_direct_mem_ptr);
    irq_line.register_transport_dbg(this, &CPU::transport_dbg);
    
    // Start CPU thread
    SC_THREAD(cpu_thread);
    
    LOG_INFO("CPU initialized");
}

void CPU::cpu_thread()
{
    // Perform ARM M-series reset with vector table initialization
    if (m_pc == 0) {  // Only on first start
        reset_from_vector_table();
    }
    
    while (true) {
        try {
            // Check for pending exceptions (in priority order)
            check_pending_exceptions();
            
            // Get current PC
            m_pc = m_registers->get_pc();
            
            // Fetch instruction (always fetch 32-bit to check for 32-bit instructions)
            uint32_t instruction_data = fetch_instruction(m_pc);
            
            bool is_32bit = m_instruction->is_32bit_instruction(instruction_data);

            // Decode instruction
            InstructionFields fields = m_instruction->decode(instruction_data, is_32bit);

            // Execute instruction
            bool pc_changed = m_execute->execute_instruction(fields, &data_bus);
            
            // Update PC if not changed by instruction (branch, etc.)
            if (!pc_changed) {
                // Increment PC based on instruction size
                m_registers->set_pc(m_pc + (is_32bit ? 4 : 2));
            }
            
            // Update performance counters
            Performance::getInstance().increment_instructions_executed();
            
            // Simulate one cycle delay
            wait(1, SC_NS);
            
        } catch (const std::exception& e) {
            LOG_ERROR("CPU exception: " + std::string(e.what()));
            break;
        }
    }
}

uint32_t CPU::fetch_instruction(uint32_t address)
{
    tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    uint32_t instruction = 0;
    
    // Set up transaction
    trans.set_command(TLM_READ_COMMAND);
    trans.set_address(address);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&instruction));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
    
    // Call transport
    inst_bus->b_transport(trans, delay);
    
    if (trans.get_response_status() != TLM_OK_RESPONSE) {
        LOG_ERROR("Instruction fetch failed at address 0x" + 
                 std::to_string(address));
        return 0;
    }
    
    wait(delay);
    return instruction;
}

uint32_t CPU::read_memory_word(uint32_t address)
{
    tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    uint32_t data = 0;
    
    // Set up transaction
    trans.set_command(TLM_READ_COMMAND);
    trans.set_address(address);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
    
    // Call transport via data bus
    data_bus->b_transport(trans, delay);
    
    if (trans.get_response_status() != TLM_OK_RESPONSE) {
        LOG_ERROR("Memory read failed at address 0x" + 
                 std::to_string(address));
        return 0;
    }
    
    wait(delay);
    return data;
}

void CPU::reset_from_vector_table()
{
    LOG_INFO("Resetting CPU from vector table");
    
    // Reset registers to default values first
    m_registers->reset();
    
    // Read initial stack pointer from vector table address 0x0
    uint32_t initial_sp = read_memory_word(0x00000000);
    if (initial_sp != 0) {
        m_registers->set_sp(initial_sp);
        std::stringstream ss;
        ss << "Set initial SP from vector table: 0x" << std::hex << initial_sp;
        LOG_INFO(ss.str());
    } else {
        LOG_WARNING("Vector table SP is 0, using default");
    }
    
    // Read reset vector (initial PC) from vector table address 0x4
    uint32_t reset_vector = read_memory_word(0x00000004);
    if (reset_vector != 0) {
        // ARM Thumb mode requires PC to have bit 0 clear for proper execution
        uint32_t reset_pc = reset_vector & 0xFFFFFFFE;  // Clear bit 0 (Thumb bit)
        m_registers->set_pc(reset_pc);
        m_pc = reset_pc;  // Update internal PC copy
        std::stringstream ss;
        ss << "Set initial PC from reset vector: 0x" << std::hex << reset_pc 
           << " (raw vector: 0x" << std::hex << reset_vector << ")";
        LOG_INFO(ss.str());
    } else {
        LOG_WARNING("Reset vector is 0, using default PC");
    }
}

void CPU::handle_irq()
{
    LOG_INFO("Handling IRQ");
    
    // Save current context
    uint32_t current_pc = m_registers->get_pc();
    m_registers->set_lr(current_pc);
    
    // Jump to IRQ handler (simplified - should use vector table)
    m_registers->set_pc(0x00000004);
    
    Performance::getInstance().increment_irq_count();
}

void CPU::b_transport(tlm_generic_payload& trans, sc_time& delay)
{
    // Handle exception signals from NVIC
    if (trans.get_command() == TLM_WRITE_COMMAND) {
        uint32_t* data = reinterpret_cast<uint32_t*>(trans.get_data_ptr());
        uint32_t exception_type = *data;
        
        // Handle different exception types
        switch (exception_type) {
            case 1: // Legacy IRQ signal
                m_irq_pending = true;
                LOG_DEBUG("Legacy IRQ signal received");
                break;
            case EXCEPTION_NMI:
                m_nmi_pending = true;
                LOG_DEBUG("NMI exception received");
                break;
            case EXCEPTION_HARD_FAULT:
                m_hardfault_pending = true;
                LOG_DEBUG("HardFault exception received");
                break;
            case EXCEPTION_PENDSV:
                m_pendsv_pending = true;
                LOG_DEBUG("PendSV exception received");
                break;
            case EXCEPTION_SYSTICK:
                m_systick_pending = true;
                LOG_DEBUG("SysTick exception received");
                break;
            default:
                if (exception_type >= EXCEPTION_IRQ0) {
                    m_irq_pending = true;
                    LOG_DEBUG("External IRQ " + std::to_string(exception_type - EXCEPTION_IRQ0) + " received");
                } else {
                    LOG_WARNING("Unknown exception type: " + std::to_string(exception_type));
                }
                break;
        }
    }
    
    trans.set_response_status(TLM_OK_RESPONSE);
}

bool CPU::get_direct_mem_ptr(tlm_generic_payload& trans, tlm_dmi& dmi_data)
{
    return false;
}

unsigned int CPU::transport_dbg(tlm_generic_payload& trans)
{
    return 0;
}

void CPU::check_pending_exceptions()
{
    // Check exceptions in priority order (ARM Cortex-M0 exception priorities)
    // NMI has highest priority (except Reset)
    if (m_nmi_pending) {
        handle_exception(EXCEPTION_NMI);
        m_nmi_pending = false;
        return;
    }
    
    // HardFault
    if (m_hardfault_pending) {
        handle_exception(EXCEPTION_HARD_FAULT);
        m_hardfault_pending = false;
        return;
    }
    
    // SysTick
    if (m_systick_pending) {
        handle_exception(EXCEPTION_SYSTICK);
        m_systick_pending = false;
        return;
    }
    
    // PendSV (lowest priority)
    if (m_pendsv_pending) {
        handle_exception(EXCEPTION_PENDSV);
        m_pendsv_pending = false;
        return;
    }
    
    // External IRQ (handled by existing IRQ mechanism for now)
    if (m_irq_pending) {
        handle_irq();
        m_irq_pending = false;
        return;
    }
}

void CPU::handle_exception(ExceptionType exception_type)
{
    LOG_INFO("Handling exception type: " + std::to_string(exception_type));
    
    // Save current context by pushing stack frame
    uint32_t return_address = m_registers->get_pc();
    push_exception_stack_frame(return_address);
    
    // Get exception vector address
    uint32_t vector_address = get_exception_vector_address(exception_type);
    
    // Jump to exception handler
    uint32_t handler_address = read_memory_word(vector_address);
    if (handler_address != 0) {
        // Clear bit 0 (Thumb bit) for proper execution
        handler_address = handler_address & 0xFFFFFFFE;
        m_registers->set_pc(handler_address);
        LOG_INFO("Jumping to exception handler at: 0x" + std::to_string(handler_address));
    } else {
        LOG_WARNING("Exception vector is 0, triggering HardFault");
        if (exception_type != EXCEPTION_HARD_FAULT) {
            trigger_exception(EXCEPTION_HARD_FAULT);
        }
    }
    
    Performance::getInstance().increment_irq_count();
}

void CPU::trigger_exception(ExceptionType exception_type)
{
    switch (exception_type) {
        case EXCEPTION_NMI:
            m_nmi_pending = true;
            break;
        case EXCEPTION_HARD_FAULT:
            m_hardfault_pending = true;
            break;
        case EXCEPTION_PENDSV:
            m_pendsv_pending = true;
            break;
        case EXCEPTION_SYSTICK:
            m_systick_pending = true;
            break;
        default:
            LOG_WARNING("Unknown exception type: " + std::to_string(exception_type));
            break;
    }
}

uint32_t CPU::get_exception_vector_address(ExceptionType exception_type)
{
    // ARM Cortex-M0 vector table is at address 0x00000000
    // Each vector is 4 bytes (32-bit address)
    return exception_type * 4;
}

void CPU::push_exception_stack_frame(uint32_t return_address)
{
    // ARM Cortex-M0 exception stack frame (8 words pushed automatically)
    uint32_t sp = m_registers->get_sp();
    
    // Push registers to stack (PSR, PC, LR, R12, R3, R2, R1, R0)
    sp -= 4; write_memory_word(sp, m_registers->get_psr());         // PSR
    sp -= 4; write_memory_word(sp, return_address);                 // PC (return address)
    sp -= 4; write_memory_word(sp, m_registers->get_lr());          // LR
    sp -= 4; write_memory_word(sp, m_registers->read_register(12)); // R12
    sp -= 4; write_memory_word(sp, m_registers->read_register(3));  // R3
    sp -= 4; write_memory_word(sp, m_registers->read_register(2));  // R2
    sp -= 4; write_memory_word(sp, m_registers->read_register(1));  // R1
    sp -= 4; write_memory_word(sp, m_registers->read_register(0));  // R0
    
    // Update stack pointer
    m_registers->set_sp(sp);
    
    // Set LR to exception return value (0xFFFFFFF9 for thread mode, main stack)
    m_registers->set_lr(0xFFFFFFF9);
}

void CPU::write_memory_word(uint32_t address, uint32_t data)
{
    tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    
    trans.set_command(TLM_WRITE_COMMAND);
    trans.set_address(address);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
    
    data_bus->b_transport(trans, delay);
    
    if (trans.get_response_status() != TLM_OK_RESPONSE) {
        LOG_ERROR("Memory write failed at address: 0x" + std::to_string(address));
    }
}