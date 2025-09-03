#include "CPU.h"
#include "Execute.h"
#include "Performance.h"
#include "Log.h"
#include "GDBServer.h"
#include <sstream>
#include <stdexcept>
#include <cstring>

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
    m_hardfault_pending(false),
    m_svc_pending(false),
    m_pending_external_exception(0),
    m_debug_mode(false),
    m_single_step(false),
    m_debug_paused(false),
    m_gdb_server(nullptr)
{
    // Initialize sub-modules
    m_registers = new Registers("registers");
    m_instruction = new Instruction("instruction");
    m_execute = new Execute("execute", m_registers);
    m_execute->set_cpu(this);
    
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
            // Debug mode handling - wait only when we need to pause
            if (m_debug_mode && m_gdb_server) {
                // Only wait when explicitly paused (not during single-step)
                if (m_debug_paused) {
                    m_gdb_server->wait_for_continue();
                    m_debug_paused = false; // Clear pause flag after continuing
                }
                
                // Check if we should exit due to server stopping
                if (!m_gdb_server->is_running()) {
                    break;
                }
            }
            
            // Check for pending exceptions (in priority order)
            check_pending_exceptions();
            
            // Get current PC
            m_pc = m_registers->get_pc();
            
            // Check for breakpoints in debug mode
            if (m_debug_mode && m_gdb_server && check_breakpoint(m_pc)) {
                // Hit a breakpoint - notify GDB and pause
                m_gdb_server->notify_breakpoint();
                m_debug_paused = true;
                continue; // Go back to wait for continue
            }
            
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
            
            // Handle single step in debug mode: send stop and pause
            // For branches, we want to stop at the target address, not the branch instruction itself
            if (m_debug_mode && m_single_step && m_gdb_server) {
                // Always stop after executing one instruction, regardless of whether it was a branch
                LOG_DEBUG("Single step completed, notifying GDB and pausing");
                m_gdb_server->notify_step_complete();
                m_single_step = false;
                m_debug_paused = true; // ensure we pause on next loop
                continue;
            }
            
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
    // DMI fast path
    if (m_inst_dmi_valid && address >= m_inst_dmi.get_start_address() && address + 3 <= m_inst_dmi.get_end_address()) {
        auto base = m_inst_dmi.get_dmi_ptr();
        uint64_t off = static_cast<uint64_t>(address) - static_cast<uint64_t>(m_inst_dmi.get_start_address());
        uint32_t val = 0;
        std::memcpy(&val, base + off, sizeof(uint32_t));
        wait(m_inst_dmi.get_read_latency());
        return val;
    }

    // Try to acquire DMI for this address
    tlm_generic_payload dmi_req;
    tlm_dmi dmi_data;
    dmi_req.set_command(TLM_READ_COMMAND);
    dmi_req.set_address(address);
    if (inst_bus->get_direct_mem_ptr(dmi_req, dmi_data)) {
        m_inst_dmi = dmi_data;
        m_inst_dmi_valid = true;
        if (address >= m_inst_dmi.get_start_address() && address + 3 <= m_inst_dmi.get_end_address()) {
            auto base = m_inst_dmi.get_dmi_ptr();
            uint64_t off = static_cast<uint64_t>(address) - static_cast<uint64_t>(m_inst_dmi.get_start_address());
            uint32_t val = 0;
            std::memcpy(&val, base + off, sizeof(uint32_t));
            wait(m_inst_dmi.get_read_latency());
            return val;
        }
    }

    // Fallback to regular TLM
    tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    uint32_t instruction = 0;
    trans.set_command(TLM_READ_COMMAND);
    trans.set_address(address);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&instruction));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(true);
    trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
    inst_bus->b_transport(trans, delay);
    if (trans.get_response_status() != TLM_OK_RESPONSE) {
        std::stringstream ss; ss << "Instruction fetch failed at address 0x" << std::hex << address;
        LOG_ERROR(ss.str());
        return 0;
    }
    wait(delay);
    return instruction;
}

uint32_t CPU::read_memory_word(uint32_t address)
{
    // DMI fast path (data bus)
    if (m_data_dmi_valid && address >= m_data_dmi.get_start_address() && address + 3 <= m_data_dmi.get_end_address()) {
        auto base = m_data_dmi.get_dmi_ptr();
        uint64_t off = static_cast<uint64_t>(address) - static_cast<uint64_t>(m_data_dmi.get_start_address());
        uint32_t val = 0;
        std::memcpy(&val, base + off, sizeof(uint32_t));
        wait(m_data_dmi.get_read_latency());
        return val;
    }

    // Try to acquire DMI for this address on data bus
    tlm_generic_payload dmi_req;
    tlm_dmi dmi_data;
    dmi_req.set_command(TLM_READ_COMMAND);
    dmi_req.set_address(address);
    if (data_bus->get_direct_mem_ptr(dmi_req, dmi_data)) {
        m_data_dmi = dmi_data;
        m_data_dmi_valid = true;
        if (address >= m_data_dmi.get_start_address() && address + 3 <= m_data_dmi.get_end_address()) {
            auto base = m_data_dmi.get_dmi_ptr();
            uint64_t off = static_cast<uint64_t>(address) - static_cast<uint64_t>(m_data_dmi.get_start_address());
            uint32_t val = 0;
            std::memcpy(&val, base + off, sizeof(uint32_t));
            wait(m_data_dmi.get_read_latency());
            return val;
        }
    }

    // Fallback to regular TLM
    tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    uint32_t data = 0;
    trans.set_command(TLM_READ_COMMAND);
    trans.set_address(address);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(true);
    trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
    data_bus->b_transport(trans, delay);
    if (trans.get_response_status() != TLM_OK_RESPONSE) {
        std::stringstream ss; ss << "Memory read failed at address 0x" << std::hex << address;
        LOG_ERROR(ss.str());
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

    // If NVIC provided a specific exception number (>=16), use it; otherwise default to IRQ0
    uint32_t exception_num = (m_pending_external_exception >= EXCEPTION_IRQ0)
                                 ? m_pending_external_exception
                                 : EXCEPTION_IRQ0;

    // Save context and vector via standard exception mechanism
    handle_exception(static_cast<ExceptionType>(exception_num));
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
                    m_pending_external_exception = exception_type;
                    m_irq_pending = true;
                    LOG_DEBUG("External IRQ exception received: " + std::to_string(exception_type));
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

    // SVCall
    if (m_svc_pending) {
        handle_exception(EXCEPTION_SVCALL);
        m_svc_pending = false;
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

void CPU::request_svc()
{
    m_svc_pending = true;
}

bool CPU::try_exception_return(uint32_t exc_return)
{
    // Validate EXC_RETURN per ARM: bits[31:28]==0xF, bits[27:4]==1 (SBOP)
    if ((exc_return & 0xF0000000u) != 0xF0000000u) {
        LOG_DEBUG("EXC_RETURN reject: top nibble != 0xF, value=" + [] (uint32_t v){ std::stringstream ss; ss<<std::hex<<v; return ss.str(); }(exc_return));
        return false;
    }
    if ((exc_return & 0x0FFFFFF0u) != 0x0FFFFFF0u) {
        // Some firmware may not set reserved ones; log and continue to be permissive
        LOG_DEBUG("EXC_RETURN SBOP not all ones, continuing anyway: value=" + [] (uint32_t v){ std::stringstream ss; ss<<std::hex<<v; return ss.str(); }(exc_return));
    }

    uint32_t low = exc_return & 0xFu;
    bool to_thread;
    bool use_psp;
    switch (low) {
        case 0x1: // Return to Handler, use MSP
            to_thread = false; use_psp = false; break;
        case 0x9: // Return to Thread, use MSP
            to_thread = true;  use_psp = false; break;
        case 0xD: // Return to Thread, use PSP
            to_thread = true;  use_psp = true;  break;
        default:
            LOG_DEBUG("EXC_RETURN reject: low nibble unsupported: 0x" + [] (uint32_t v){ std::stringstream ss; ss<<std::hex<<v; return ss.str(); }(low));
            return false; // Unused/Reserved
    }

    // Select stack pointer for unstacking
    uint32_t sp = use_psp ? m_registers->get_psp() : m_registers->get_msp();

    // Pop the standard exception stack frame (R0-R3, R12, LR, PC, PSR)
    uint32_t r0  = read_memory_word(sp);      sp += 4;
    uint32_t r1  = read_memory_word(sp);      sp += 4;
    uint32_t r2  = read_memory_word(sp);      sp += 4;
    uint32_t r3  = read_memory_word(sp);      sp += 4;
    uint32_t r12 = read_memory_word(sp);      sp += 4;
    uint32_t lr  = read_memory_word(sp);      sp += 4;
    uint32_t pc  = read_memory_word(sp);      sp += 4;
    uint32_t psr = read_memory_word(sp);      sp += 4;

    // Write back the updated SP to the selected stack
    if (use_psp) {
        m_registers->set_psp(sp);
    } else {
        m_registers->set_msp(sp);
    }

    // Restore core registers
    m_registers->write_register(0, r0);
    m_registers->write_register(1, r1);
    m_registers->write_register(2, r2);
    m_registers->write_register(3, r3);
    m_registers->write_register(12, r12);
    m_registers->set_lr(lr);
    m_registers->set_psr(psr);

    // If returning to Thread mode, switch SP selection (CONTROL.SPSEL)
    if (to_thread) {
        uint32_t control = m_registers->get_control();
        if (use_psp) control |=  (1u << 1); // SPSEL=1 -> PSP
        else         control &= ~(1u << 1); // SPSEL=0 -> MSP
        m_registers->set_control(control);
    }

    // Branch to restored PC (ensure Thumb bit handling) for both Thread and Handler returns
    m_registers->set_pc(pc & ~1u);
    LOG_INFO("Exception return performed to PC: 0x" + [] (uint32_t v){ std::stringstream ss; ss<<std::hex<<v; return ss.str(); }(pc));
    return true;
}

void CPU::handle_exception(ExceptionType exception_type)
{
    LOG_INFO("Handling exception type: " + std::to_string(exception_type));
    
    // Save current context by pushing stack frame
    uint32_t return_address = m_registers->get_pc();
    push_exception_stack_frame(return_address);
    
    // Enter Handler mode: set IPSR to exception number and force MSP as active SP
    m_registers->set_ipsr(static_cast<uint32_t>(exception_type));
    // In Handler mode, hardware uses MSP regardless of CONTROL.SPSEL; we approximate by clearing SPSEL
    uint32_t control = m_registers->get_control();
    control &= ~(1u << 1); // SPSEL=0 -> MSP
    m_registers->set_control(control);
    
    // Get exception vector address
    uint32_t vector_address = get_exception_vector_address(exception_type);
    
    // Jump to exception handler
    uint32_t handler_address = read_memory_word(vector_address);
    if (handler_address != 0) {
        // Clear bit 0 (Thumb bit) for proper execution
        handler_address = handler_address & 0xFFFFFFFE;
        m_registers->set_pc(handler_address);
        std::stringstream ss; ss << "Jumping to exception handler at: 0x" << std::hex << handler_address;
        LOG_INFO(ss.str());
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
    // ARM Cortex-M exception entry behavior:
    // - If coming from Thread mode, hardware stacks to the current thread SP (MSP if SPSEL=0, PSP if SPSEL=1)
    // - If coming from Handler mode, stacks to MSP
    bool in_handler = (m_registers->get_ipsr() != 0);
    bool thread_used_psp = (!in_handler) && ((m_registers->get_control() & (1u<<1)) != 0);

    // Select stack for stacking
    uint32_t sp = thread_used_psp ? m_registers->get_psp() : m_registers->get_msp();

    // Push registers to chosen stack (PSR, PC, LR, R12, R3, R2, R1, R0)
    sp -= 4; write_memory_word(sp, m_registers->get_psr());         // xPSR
    sp -= 4; write_memory_word(sp, return_address);                 // PC (return address)
    sp -= 4; write_memory_word(sp, m_registers->get_lr());          // LR
    sp -= 4; write_memory_word(sp, m_registers->read_register(12)); // R12
    sp -= 4; write_memory_word(sp, m_registers->read_register(3));  // R3
    sp -= 4; write_memory_word(sp, m_registers->read_register(2));  // R2
    sp -= 4; write_memory_word(sp, m_registers->read_register(1));  // R1
    sp -= 4; write_memory_word(sp, m_registers->read_register(0));  // R0

    // Write back the updated stack pointer to the correct bank
    if (thread_used_psp) {
        m_registers->set_psp(sp);
        // LR encoding for return to Thread using PSP
        m_registers->set_lr(0xFFFFFFFDu);
    } else if (in_handler) {
        m_registers->set_msp(sp);
        // LR encoding for return to Handler using MSP
        m_registers->set_lr(0xFFFFFFF1u);
    } else {
        m_registers->set_msp(sp);
        // LR encoding for return to Thread using MSP
        m_registers->set_lr(0xFFFFFFF9u);
    }
}

void CPU::write_memory_word(uint32_t address, uint32_t data)
{
    // DMI fast path for writes
    if (m_data_dmi_valid && address >= m_data_dmi.get_start_address() && address + 3 <= m_data_dmi.get_end_address()) {
        if (m_data_dmi.is_write_allowed()) {
            auto base = m_data_dmi.get_dmi_ptr();
            uint64_t off = static_cast<uint64_t>(address) - static_cast<uint64_t>(m_data_dmi.get_start_address());
            std::memcpy(base + off, &data, sizeof(uint32_t));
            wait(m_data_dmi.get_write_latency());
            return;
        }
    }

    // Try to acquire DMI for writes
    tlm_generic_payload dmi_req;
    tlm_dmi dmi_data;
    dmi_req.set_command(TLM_WRITE_COMMAND);
    dmi_req.set_address(address);
    if (data_bus->get_direct_mem_ptr(dmi_req, dmi_data)) {
        m_data_dmi = dmi_data;
        m_data_dmi_valid = true;
        if (address >= m_data_dmi.get_start_address() && address + 3 <= m_data_dmi.get_end_address() && m_data_dmi.is_write_allowed()) {
            auto base = m_data_dmi.get_dmi_ptr();
            uint64_t off = static_cast<uint64_t>(address) - static_cast<uint64_t>(m_data_dmi.get_start_address());
            std::memcpy(base + off, &data, sizeof(uint32_t));
            wait(m_data_dmi.get_write_latency());
            return;
        }
    }

    // Fallback to regular TLM
    tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    trans.set_command(TLM_WRITE_COMMAND);
    trans.set_address(address);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(4);
    trans.set_streaming_width(4);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(true);
    trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
    data_bus->b_transport(trans, delay);
    if (trans.get_response_status() != TLM_OK_RESPONSE) {
        std::stringstream ss; ss << "Memory write failed at address: 0x" << std::hex << address;
        LOG_ERROR(ss.str());
    }
}

// Debug interface methods for GDB server
uint32_t CPU::read_memory_debug(uint32_t address)
{
    tlm_generic_payload trans;
    uint8_t data = 0;
    sc_time delay = SC_ZERO_TIME;
    
    trans.set_command(TLM_READ_COMMAND);
    trans.set_address(address);
    trans.set_data_ptr(&data);
    trans.set_data_length(1);
    trans.set_streaming_width(1);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
    
    data_bus->b_transport(trans, delay);
    
    if (trans.get_response_status() != TLM_OK_RESPONSE) {
        throw std::runtime_error("Debug memory read failed");
    }
    
    return static_cast<uint32_t>(data);
}

void CPU::write_memory_debug(uint32_t address, uint8_t data)
{
    tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    
    trans.set_command(TLM_WRITE_COMMAND);
    trans.set_address(address);
    trans.set_data_ptr(&data);
    trans.set_data_length(1);
    trans.set_streaming_width(1);
    trans.set_byte_enable_ptr(0);
    trans.set_dmi_allowed(false);
    trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
    
    data_bus->b_transport(trans, delay);
    
    if (trans.get_response_status() != TLM_OK_RESPONSE) {
        throw std::runtime_error("Debug memory write failed");
    }
}

bool CPU::check_breakpoint(uint32_t address) const
{
    // Check if there's a breakpoint at this address via GDB server
    if (m_gdb_server) {
        return m_gdb_server->has_breakpoint(address);
    }
    return false;
}