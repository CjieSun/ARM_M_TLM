#include "Execute.h"
#include "Performance.h"
#include "Log.h"

Execute::Execute(sc_module_name name, Registers* registers) : 
    sc_module(name), m_registers(registers)
{
    LOG_INFO("Execute unit initialized");
}

bool Execute::execute_instruction(const InstructionFields& fields, void* data_bus)
{
    bool pc_changed = false;
    
    switch (fields.type) {
        case INST_BRANCH:
            pc_changed = execute_branch(fields);
            break;
        case INST_DATA_PROCESSING:
            pc_changed = execute_data_processing(fields);
            break;
        case INST_LOAD_STORE:
            pc_changed = execute_load_store(fields, data_bus);
            break;
        case INST_LOAD_STORE_MULTIPLE:
            pc_changed = execute_load_store_multiple(fields, data_bus);
            break;
        case INST_STATUS_REGISTER:
            pc_changed = execute_status_register(fields);
            break;
        case INST_MISCELLANEOUS:
            pc_changed = execute_miscellaneous(fields);
            break;
        case INST_EXCEPTION:
            pc_changed = execute_exception(fields);
            break;
        default:
            LOG_WARNING("Unknown instruction type");
            break;
    }
    
    return pc_changed;
}

bool Execute::execute_branch(const InstructionFields& fields)
{
    // Check condition
    if (fields.cond != 0xE && !check_condition(fields.cond)) {
        return false;  // Condition not met, don't branch
    }
    
    uint32_t current_pc = m_registers->get_pc();
    int32_t offset = static_cast<int32_t>(fields.imm) * 2;  // Thumb uses 2-byte offsets
    uint32_t new_pc = current_pc + 4 + offset;  // PC is already +4 in ARM
    
    m_registers->set_pc(new_pc);
    Performance::getInstance().increment_branches_taken();
    
    LOG_DEBUG("Branch taken to 0x" + std::to_string(new_pc));
    return true;
}

bool Execute::execute_data_processing(const InstructionFields& fields)
{
    // Simplified ALU operations
    uint32_t op1 = m_registers->read_register(fields.rn);
    uint32_t op2 = m_registers->read_register(fields.rm);
    uint32_t result = 0;
    
    // Determine operation based on opcode bits
    uint8_t op = (fields.opcode >> 9) & 0x7;
    
    switch (op) {
        case 0: // MOV
            result = op2;
            break;
        case 1: // ADD
            result = op1 + op2;
            break;
        case 2: // SUB
            result = op1 - op2;
            break;
        case 3: // AND
            result = op1 & op2;
            break;
        case 4: // ORR
            result = op1 | op2;
            break;
        case 5: // EOR
            result = op1 ^ op2;
            break;
        case 6: // CMP (no writeback)
            result = op1 - op2;
            update_flags(result, false, false);
            return false;
        case 7: // TST (no writeback)
            result = op1 & op2;
            update_flags(result, false, false);
            return false;
        default:
            result = op2;
            break;
    }
    
    m_registers->write_register(fields.rd, result);
    
    if (fields.s_bit) {
        update_flags(result, false, false);
    }
    
    return false;
}

bool Execute::execute_load_store(const InstructionFields& fields, void* data_bus)
{
    uint32_t base_addr = m_registers->read_register(fields.rn);
    uint32_t address = base_addr + (fields.imm * 4);  // Word-aligned
    
    // Determine if load or store based on opcode
    bool is_load = (fields.opcode & 0x0800) != 0;
    
    if (is_load) {
        uint32_t data = read_memory(address, 4, data_bus);
        m_registers->write_register(fields.rd, data);
        LOG_DEBUG("Load: R" + std::to_string(fields.rd) + " = [0x" + std::to_string(address) + "] = 0x" + std::to_string(data));
    } else {
        uint32_t data = m_registers->read_register(fields.rd);
        write_memory(address, data, 4, data_bus);
        LOG_DEBUG("Store: [0x" + std::to_string(address) + "] = R" + std::to_string(fields.rd) + " = 0x" + std::to_string(data));
    }
    
    return false;
}

bool Execute::execute_load_store_multiple(const InstructionFields& fields, void* data_bus)
{
    uint32_t base_addr = m_registers->read_register(fields.rn);
    uint32_t address = base_addr;
    
    // Determine if load or store
    bool is_load = (fields.opcode & 0x0800) != 0;
    
    for (int i = 0; i < 8; i++) {
        if (fields.imm & (1 << i)) {
            if (is_load) {
                uint32_t data = read_memory(address, 4, data_bus);
                m_registers->write_register(i, data);
            } else {
                uint32_t data = m_registers->read_register(i);
                write_memory(address, data, 4, data_bus);
            }
            address += 4;
        }
    }
    
    // Update base register
    m_registers->write_register(fields.rn, address);
    
    return false;
}

bool Execute::execute_status_register(const InstructionFields& fields)
{
    // Simplified status register operations
    return false;
}

bool Execute::execute_miscellaneous(const InstructionFields& fields)
{
    // Handle miscellaneous instructions
    return false;
}

bool Execute::execute_exception(const InstructionFields& fields)
{
    // Handle SVC instruction
    LOG_INFO("SVC instruction executed");
    
    // For now, just continue execution
    return false;
}

bool Execute::check_condition(uint8_t condition)
{
    // Simplified condition checking
    switch (condition) {
        case 0x0: // EQ
            return m_registers->get_z_flag();
        case 0x1: // NE
            return !m_registers->get_z_flag();
        case 0xE: // AL (always)
            return true;
        default:
            return true;  // Default to true for simplicity
    }
}

void Execute::update_flags(uint32_t result, bool carry, bool overflow)
{
    m_registers->set_n_flag((result & 0x80000000) != 0);
    m_registers->set_z_flag(result == 0);
    m_registers->set_c_flag(carry);
    m_registers->set_v_flag(overflow);
}

uint32_t Execute::read_memory(uint32_t address, uint32_t size, void* socket)
{
    tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    uint32_t data = 0;
    
    trans.set_command(TLM_READ_COMMAND);
    trans.set_address(address);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(size);
    trans.set_streaming_width(size);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
    
    // Note: This is a simplified implementation
    // In reality, we'd need to properly handle the socket calling
    
    Performance::getInstance().increment_memory_reads();
    
    if (Log::getInstance().get_log_level() >= LOG_TRACE) {
        Log::getInstance().log_memory_access(address, data, size, false);
    }
    
    return data;
}

void Execute::write_memory(uint32_t address, uint32_t data, uint32_t size, void* socket)
{
    tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    
    trans.set_command(TLM_WRITE_COMMAND);
    trans.set_address(address);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(size);
    trans.set_streaming_width(size);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(false);
    trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
    
    // Note: This is a simplified implementation
    
    Performance::getInstance().increment_memory_writes();
    
    if (Log::getInstance().get_log_level() >= LOG_TRACE) {
        Log::getInstance().log_memory_access(address, data, size, true);
    }
}