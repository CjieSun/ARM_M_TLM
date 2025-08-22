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
        case INST_BRANCH_COND:
            pc_changed = execute_branch(fields);
            break;
        case INST_BRANCH_UNCOND:
            pc_changed = execute_branch(fields);
            break;
        case INST_BRANCH_LINK:
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
        case INST_SWI:
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
    // Check condition for conditional branches
    if (fields.cond != 0xE && !check_condition(fields.cond)) {
        return false;  // Condition not met, don't branch
    }
    
    uint32_t current_pc = m_registers->get_pc();
    uint32_t new_pc;
    
    if ((fields.opcode & 0xFC00) == 0x4400 && fields.alu_op == 3) {
        // BX/BLX instruction (Hi register operations format)
        new_pc = m_registers->read_register(fields.rm);
        
        // Check if this is BLX (bit 7 set in original encoding would indicate BLX)
        if ((fields.opcode & 0x0080) != 0) {
            // BLX - save return address in LR
            m_registers->write_register(14, current_pc + 2 + 1); // +1 for Thumb bit
        }
        
        // BX/BLX can switch between ARM and Thumb modes
        // Bit 0 indicates Thumb mode (which we always use)
        m_registers->set_pc(new_pc & ~1);
        
        LOG_DEBUG("BX/BLX to 0x" + std::to_string(new_pc));
    } else if (fields.alu_op == 1) {
        // BL instruction (first half already processed)
        // For simplicity, handle as regular branch for now
        // In a complete implementation, this would be a 32-bit instruction
        new_pc = current_pc + 4 + (static_cast<int32_t>(fields.imm) * 2);
        
        // Save return address in LR
        m_registers->write_register(14, current_pc + 4 + 1); // +1 for Thumb bit
        m_registers->set_pc(new_pc);
        
        LOG_DEBUG("BL to 0x" + std::to_string(new_pc));
    } else {
        // Regular branch (B or B<cond>)
        int32_t offset = static_cast<int32_t>(fields.imm);
        new_pc = current_pc + 4 + offset;
        
        m_registers->set_pc(new_pc);
        LOG_DEBUG("Branch taken to 0x" + std::to_string(new_pc));
    }
    
    Performance::getInstance().increment_branches_taken();
    return true;
}

bool Execute::execute_data_processing(const InstructionFields& fields)
{
    // Handle different data processing formats
    uint32_t result = 0;
    bool carry = false;
    bool overflow = false;
    uint32_t op1, op2;
    
    // Get operands based on instruction format
    if (fields.rm == 0xFF) {
        // Immediate operand
        op1 = m_registers->read_register(fields.rn);
        op2 = fields.imm;
    } else if ((fields.opcode & 0xFC00) == 0x4400) {
        // Hi register operations (Format 5)
        op1 = m_registers->read_register(fields.rd);
        op2 = m_registers->read_register(fields.rm);
    } else if ((fields.opcode & 0xF000) == 0xA000) {
        // Load address (Format 12) - ADD PC/SP + immediate
        if (fields.rn == 15) {
            // PC-relative: align PC to word boundary
            op1 = (m_registers->get_pc() + 4) & ~3;
        } else {
            op1 = m_registers->read_register(fields.rn);
        }
        op2 = fields.imm;
        result = op1 + op2;
        m_registers->write_register(fields.rd, result);
        return false;
    } else if ((fields.opcode & 0xE000) == 0x0000 && (fields.opcode & 0x1800) != 0x1800) {
        // Shifted register operations (Format 1)
        op2 = m_registers->read_register(fields.rm);
        
        // Apply shift
        switch (fields.shift_type) {
            case 0: // LSL
                if (fields.shift_amount == 0) {
                    result = op2;
                } else {
                    carry = (op2 >> (32 - fields.shift_amount)) & 1;
                    result = op2 << fields.shift_amount;
                }
                break;
            case 1: // LSR
                {
                    uint32_t shift_amt = fields.shift_amount;
                    if (shift_amt == 0) shift_amt = 32;
                    carry = (op2 >> (shift_amt - 1)) & 1;
                    result = op2 >> shift_amt;
                }
                break;
            case 2: // ASR
                {
                    uint32_t shift_amt = fields.shift_amount;
                    if (shift_amt == 0) shift_amt = 32;
                    carry = (op2 >> (shift_amt - 1)) & 1;
                    result = static_cast<int32_t>(op2) >> shift_amt;
                }
                break;
            default:
                result = op2;
                break;
        }
        
        m_registers->write_register(fields.rd, result);
        update_flags(result, carry, overflow);
        return false;
    } else {
        // Regular ALU operations
        op1 = m_registers->read_register(fields.rn);
        op2 = m_registers->read_register(fields.rm);
    }
    
    // Determine operation based on ALU op code
    switch (fields.alu_op) {
        case 0: // MOV (Format 3) or AND (Format 4)
            if ((fields.opcode & 0xE000) == 0x2000) {
                result = op2; // MOV immediate
            } else {
                result = op1 & op2; // AND
            }
            break;
        case 1: // ADD
            result = op1 + op2;
            carry = (result < op1); // Detect carry
            overflow = ((op1 ^ result) & (op2 ^ result)) >> 31; // Detect overflow
            break;
        case 2: // SUB
            result = op1 - op2;
            carry = (op1 >= op2); // Borrow is inverse of carry
            overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
            break;
        case 3: // CMP (Format 3) or EOR (Format 4)
            if ((fields.opcode & 0xE000) == 0x2000) {
                result = op1 - op2; // CMP
                carry = (op1 >= op2);
                overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
                update_flags(result, carry, overflow);
                return false; // No writeback for CMP
            } else {
                result = op1 ^ op2; // EOR
            }
            break;
        case 4: // LSL (Format 4)
            carry = op1 >> (32 - (op2 & 0xFF));
            result = op1 << (op2 & 0xFF);
            break;
        case 5: // LSR (Format 4)
            carry = op1 >> ((op2 & 0xFF) - 1);
            result = op1 >> (op2 & 0xFF);
            break;
        case 6: // ASR (Format 4)
            carry = static_cast<int32_t>(op1) >> ((op2 & 0xFF) - 1);
            result = static_cast<int32_t>(op1) >> (op2 & 0xFF);
            break;
        case 8: // TST (Format 4)
            result = op1 & op2;
            update_flags(result, false, false);
            return false; // No writeback
        case 9: // NEG (Format 4)
            result = 0 - op2;
            break;
        case 10: // CMP (Format 4)
            result = op1 - op2;
            carry = (op1 >= op2);
            overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
            update_flags(result, carry, overflow);
            return false; // No writeback
        case 12: // ORR (Format 4)
            result = op1 | op2;
            break;
        case 13: // MUL (Format 4)
            result = op1 * op2;
            break;
        case 14: // BIC (Format 4)
            result = op1 & (~op2);
            break;
        case 15: // MVN (Format 4)
            result = ~op2;
            break;
        default:
            result = op2; // Default MOV
            break;
    }
    
    // Writeback result and update flags
    m_registers->write_register(fields.rd, result);
    
    if (fields.s_bit || (fields.opcode & 0xFC00) == 0x4000) {
        update_flags(result, carry, overflow);
    }
    
    return false;
}

bool Execute::execute_load_store(const InstructionFields& fields, void* data_bus)
{
    uint32_t address = 0;
    uint32_t data = 0;
    
    // Calculate effective address based on addressing mode
    if (fields.rm != 0 && fields.rm != 0xFF) {
        // Register offset addressing
        uint32_t base = m_registers->read_register(fields.rn);
        uint32_t offset = m_registers->read_register(fields.rm);
        address = base + offset;
    } else {
        // Immediate offset addressing
        uint32_t base = m_registers->read_register(fields.rn);
        address = base + fields.imm;
    }
    
    // Handle special addressing modes
    if (fields.rn == 15) {
        // PC-relative addressing - align PC to word boundary
        address = (m_registers->get_pc() + 4) & ~3;
        address += fields.imm;
    }
    
    // Perform load or store based on L bit
    if (fields.load_store_bit) {
        // Load operation
        uint32_t size = 4; // Default word size
        
        if (fields.byte_word == 1) {
            // Byte load
            size = 1;
            data = read_memory(address, size, data_bus) & 0xFF;
        } else if (fields.byte_word == 2) {
            // Halfword load
            size = 2;
            data = read_memory(address, size, data_bus) & 0xFFFF;
        } else {
            // Word load
            data = read_memory(address, size, data_bus);
        }
        
        // Handle sign extension for special load operations
        if ((fields.opcode & 0xF000) == 0x5000 && (fields.opcode & 0x0200) != 0) {
            // Sign-extended loads
            switch (fields.alu_op) {
                case 1: // LDSB - Load sign-extended byte
                    data = read_memory(address, 1, data_bus);
                    if (data & 0x80) data |= 0xFFFFFF00; // Sign extend
                    break;
                case 3: // LDSH - Load sign-extended halfword
                    data = read_memory(address, 2, data_bus);
                    if (data & 0x8000) data |= 0xFFFF0000; // Sign extend
                    break;
                case 2: // LDRH - Load halfword (already handled above)
                    break;
            }
        }
        
        m_registers->write_register(fields.rd, data);
        LOG_DEBUG("Load: R" + std::to_string(fields.rd) + " = [0x" + std::to_string(address) + "] = 0x" + std::to_string(data));
    } else {
        // Store operation
        data = m_registers->read_register(fields.rd);
        uint32_t size = 4; // Default word size
        
        if (fields.byte_word == 1) {
            // Byte store
            size = 1;
            data &= 0xFF;
        } else if (fields.byte_word == 2) {
            // Halfword store
            size = 2;
            data &= 0xFFFF;
        }
        
        // Handle special store operations
        if ((fields.opcode & 0xF000) == 0x5000 && (fields.opcode & 0x0200) != 0 && fields.alu_op == 0) {
            // STRH - Store halfword
            size = 2;
            data &= 0xFFFF;
        }
        
        write_memory(address, data, size, data_bus);
        LOG_DEBUG("Store: [0x" + std::to_string(address) + "] = R" + std::to_string(fields.rd) + " = 0x" + std::to_string(data));
    }
    
    return false;
}

bool Execute::execute_load_store_multiple(const InstructionFields& fields, void* data_bus)
{
    uint32_t base_addr = m_registers->read_register(fields.rn);
    uint32_t address = base_addr;
    bool is_push_pop = (fields.opcode & 0xF600) == 0xB400;
    
    if (is_push_pop) {
        // PUSH/POP operations
        if (fields.load_store_bit) {
            // POP operation (load)
            // POP processes registers in ascending order (R0, R1, ... PC)
            for (int i = 0; i <= 15; i++) {
                if (fields.reg_list & (1 << i)) {
                    uint32_t data = read_memory(address, 4, data_bus);
                    
                    if (i < 8) {
                        m_registers->write_register(i, data);
                    } else if (i == 15 && (fields.reg_list & 0x8000)) {
                        // POP PC - this causes a branch
                        m_registers->set_pc(data & ~1); // Clear Thumb bit
                        LOG_DEBUG("POP PC: 0x" + std::to_string(data));
                        address += 4;
                        m_registers->write_register(13, address); // Update SP
                        return true; // PC changed
                    } else if (i == 14 && (fields.reg_list & 0x4000)) {
                        m_registers->write_register(14, data); // LR
                    }
                    
                    address += 4;
                }
            }
            m_registers->write_register(13, address); // Update SP
        } else {
            // PUSH operation (store)  
            // PUSH processes registers in descending order (LR, ... R1, R0)
            
            // Count registers to determine how much to decrement SP
            int reg_count = 0;
            for (int i = 0; i <= 15; i++) {
                if (fields.reg_list & (1 << i)) reg_count++;
            }
            
            address -= reg_count * 4;
            m_registers->write_register(13, address); // Update SP first
            
            uint32_t temp_addr = address;
            for (int i = 0; i <= 15; i++) {
                if (fields.reg_list & (1 << i)) {
                    uint32_t data;
                    
                    if (i < 8) {
                        data = m_registers->read_register(i);
                    } else if (i == 14 && (fields.reg_list & 0x4000)) {
                        data = m_registers->read_register(14); // LR
                    } else {
                        continue;
                    }
                    
                    write_memory(temp_addr, data, 4, data_bus);
                    temp_addr += 4;
                }
            }
        }
    } else {
        // Regular LDM/STM operations
        for (int i = 0; i < 8; i++) {
            if (fields.reg_list & (1 << i)) {
                if (fields.load_store_bit) {
                    uint32_t data = read_memory(address, 4, data_bus);
                    m_registers->write_register(i, data);
                } else {
                    uint32_t data = m_registers->read_register(i);
                    write_memory(address, data, 4, data_bus);
                }
                address += 4;
            }
        }
        
        // Update base register (writeback)
        if (!(fields.reg_list & (1 << fields.rn))) {
            m_registers->write_register(fields.rn, address);
        }
    }
    
    return false;
}

bool Execute::execute_status_register(const InstructionFields& fields)
{
    // Simplified status register operations
    return false;
}

bool Execute::execute_miscellaneous(const InstructionFields& fields)
{
    if ((fields.opcode & 0xFF00) == 0xB000) {
        // Add offset to Stack Pointer
        uint32_t sp = m_registers->read_register(13);
        uint32_t result;
        
        if (fields.alu_op == 1) {
            // ADD SP, immediate
            result = sp + fields.imm;
        } else {
            // SUB SP, immediate  
            result = sp - fields.imm;
        }
        
        m_registers->write_register(13, result);
        LOG_DEBUG("SP adjusted: SP = 0x" + std::to_string(result));
    }
    
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
    // ARM condition code checking based on CPSR flags
    bool n = m_registers->get_n_flag();
    bool z = m_registers->get_z_flag();
    bool c = m_registers->get_c_flag();
    bool v = m_registers->get_v_flag();
    
    switch (condition) {
        case 0x0: // EQ - Equal (Z set)
            return z;
        case 0x1: // NE - Not equal (Z clear)
            return !z;
        case 0x2: // CS/HS - Carry set/unsigned higher or same (C set)
            return c;
        case 0x3: // CC/LO - Carry clear/unsigned lower (C clear)
            return !c;
        case 0x4: // MI - Minus/negative (N set)
            return n;
        case 0x5: // PL - Plus/positive or zero (N clear)
            return !n;
        case 0x6: // VS - Overflow set (V set)
            return v;
        case 0x7: // VC - Overflow clear (V clear)
            return !v;
        case 0x8: // HI - Unsigned higher (C set and Z clear)
            return c && !z;
        case 0x9: // LS - Unsigned lower or same (C clear or Z set)
            return !c || z;
        case 0xA: // GE - Signed greater than or equal (N == V)
            return n == v;
        case 0xB: // LT - Signed less than (N != V)
            return n != v;
        case 0xC: // GT - Signed greater than (Z clear and N == V)
            return !z && (n == v);
        case 0xD: // LE - Signed less than or equal (Z set or N != V)
            return z || (n != v);
        case 0xE: // AL - Always
            return true;
        case 0xF: // Reserved (behaves as never in ARMv6-M)
            return false;
        default:
            return true;  // Default to true for safety
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