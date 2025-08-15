#include "CPU.h"
#include "Performance.h"
#include "Log.h"

CPU::CPU(sc_module_name name) : 
    sc_module(name),
    inst_bus("inst_bus"),
    data_bus("data_bus"),
    irq_line("irq_line"),
    m_irq_pending(false),
    m_pc(0)
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
    while (true) {
        try {
            // Handle pending IRQ
            if (m_irq_pending) {
                handle_irq();
                m_irq_pending = false;
            }
            
            // Get current PC
            m_pc = m_registers->get_pc();
            
            // Fetch instruction
            uint32_t instruction = fetch_instruction(m_pc);
            
            // Decode instruction
            InstructionFields fields = m_instruction->decode(instruction & 0xFFFF);
            
            // Log instruction execution
            if (Log::getInstance().get_log_level() >= LOG_TRACE) {
                std::stringstream ss;
                ss << "PC: 0x" << std::hex << m_pc 
                   << " INST: 0x" << std::hex << (instruction & 0xFFFF)
                   << " " << m_instruction->get_instruction_name(fields);
                Log::getInstance().log_instruction(m_pc, instruction & 0xFFFF, 
                    m_instruction->get_instruction_name(fields), ss.str());
            }
            
            // Execute instruction
            bool pc_changed = m_execute->execute_instruction(fields, &data_bus);
            
            // Update PC if not changed by instruction (branch, etc.)
            if (!pc_changed) {
                m_registers->set_pc(m_pc + 2); // Thumb instructions are 2 bytes
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
    // Handle IRQ signal
    if (trans.get_command() == TLM_WRITE_COMMAND) {
        uint32_t* data = reinterpret_cast<uint32_t*>(trans.get_data_ptr());
        if (*data == 1) {
            m_irq_pending = true;
            LOG_DEBUG("IRQ signal received");
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