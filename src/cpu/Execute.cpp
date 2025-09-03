#include "Execute.h"
#include "CPU.h" // For calling request_svc on SVC execution
#include "Performance.h"
#include "Log.h"
#include <sstream>
#include <iomanip>
#include <cstring>

// CPU is included above for request_svc; we keep using its initiator socket type too

namespace {
// Format 32-bit values as 0xhhhhhhhh
static std::string hex32(uint32_t v) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::nouppercase << std::setw(8) << std::setfill('0') << v;
    return oss.str();
}
}

Execute::Execute(sc_module_name name, Registers* registers) : 
    sc_module(name), m_registers(registers)
{
    LOG_INFO("Execute unit initialized");
}

bool Execute::execute_instruction(const InstructionFields& fields, void* data_bus)
{
    bool pc_changed = false;
    
    switch (fields.type) {
        // Branches
        case INST_T16_B_COND:
        case INST_T16_B:
        case INST_T16_BX:
        case INST_T16_BLX:
        case INST_T32_BL:
            pc_changed = execute_branch(fields);
            break;
        // Data processing
        case INST_T16_LSL_IMM:
        case INST_T16_LSR_IMM:
        case INST_T16_ASR_IMM:
        case INST_T16_ADD_REG:
        case INST_T16_SUB_REG:
        case INST_T16_ADD_IMM3:
        case INST_T16_SUB_IMM3:
        case INST_T16_MOV_IMM:
        case INST_T16_CMP_IMM:
        case INST_T16_ADD_IMM8:
        case INST_T16_SUB_IMM8:
        case INST_T16_AND:
        case INST_T16_EOR:
        case INST_T16_LSL_REG:
        case INST_T16_LSR_REG:
        case INST_T16_ASR_REG:
        case INST_T16_ADC:
        case INST_T16_SBC:
        case INST_T16_ROR:
        case INST_T16_TST:
        case INST_T16_NEG:
        case INST_T16_CMP_REG:
        case INST_T16_CMN:
        case INST_T16_ORR:
        case INST_T16_MUL:
        case INST_T16_BIC:
        case INST_T16_MVN:
        case INST_T16_ADD_HI:
        case INST_T16_CMP_HI:
        case INST_T16_MOV_HI:
            pc_changed = execute_data_processing(fields);
            break;
        case INST_T16_ADD_SP_IMM7:
        case INST_T16_SUB_SP_IMM7:
            pc_changed = execute_data_processing(fields);
            break;
        // Load/Store
        case INST_T16_LDR_PC:
        case INST_T16_STR_REG:
        case INST_T16_STRH_REG:
        case INST_T16_STRB_REG:
        case INST_T16_LDRSB_REG:
        case INST_T16_LDR_REG:
        case INST_T16_LDRH_REG:
        case INST_T16_LDRB_REG:
        case INST_T16_LDRSH_REG:
        case INST_T16_STR_IMM:
        case INST_T16_LDR_IMM:
        case INST_T16_STRB_IMM:
        case INST_T16_LDRB_IMM:
        case INST_T16_STRH_IMM:
        case INST_T16_LDRH_IMM:
        case INST_T16_STR_SP:
        case INST_T16_LDR_SP:
            pc_changed = execute_load_store(fields, data_bus);
            break;
        case INST_T16_ADD_PC:
        case INST_T16_ADD_SP:
            pc_changed = execute_data_processing(fields);
            break;
        // Extend instructions
        case INST_T16_EXTEND:
            pc_changed = execute_extend(fields);
            break;
        // CPS instructions
        case INST_T16_CPS:
            pc_changed = execute_cps(fields);
            break;
        // Multiple load/store
        case INST_T16_STMIA:
        case INST_T16_LDMIA:
        case INST_T16_PUSH:
        case INST_T16_POP:
            pc_changed = execute_load_store_multiple(fields, data_bus);
            break;
        // Status/System (none for v6-M beyond hints)
            pc_changed = execute_status_register(fields);
            break;
        case INST_T16_HINT:
        case INST_T16_BKPT:
            pc_changed = execute_miscellaneous(fields);
            break;
        case INST_T16_SVC:
            pc_changed = execute_exception(fields);
            break;
        // T32 Memory barriers
        case INST_T32_DSB:
        case INST_T32_DMB:
        case INST_T32_ISB:
            pc_changed = execute_memory_barrier(fields);
            break;
        // T32 System register access
        case INST_T32_MSR:
            pc_changed = execute_msr(fields);
            break;
        case INST_T32_MRS:
            pc_changed = execute_mrs(fields);
            break;
        default:
            LOG_WARNING("Unknown instruction type");
            break;
    }
    
    return pc_changed;
}

bool Execute::execute_branch(const InstructionFields& fields)
{
    // For conditional B only, check condition; BX/BLX/BL are always executed
    if (fields.type == INST_T16_B_COND) {
        if (!check_condition(fields.cond)) {
            return false;  // Condition not met
        }
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
        
        // BX to EXC_RETURN magic value should perform an exception return
        if (m_cpu && m_cpu->try_exception_return(new_pc)) {
            LOG_DEBUG("Exception return via BX R" + std::to_string(fields.rm));
            return true; // PC updated by exception return
        }

        // BX/BLX can switch between ARM and Thumb modes
        // Bit 0 indicates Thumb mode (which we always use)
        m_registers->set_pc(new_pc & ~1);
        
    LOG_DEBUG("BX/BLX to " + hex32(new_pc));
    } else if (fields.type == INST_T32_BL || fields.alu_op == 1) {
        // BL instruction (Thumb): PC is current + 4; fields.imm is halfword offset
        int32_t byte_off = static_cast<int32_t>(fields.imm) * 2;
        new_pc = current_pc + 4 + byte_off;

        // Save return address in LR (Thumb bit set)
        m_registers->write_register(14, current_pc + 4 + 1);
        m_registers->set_pc(new_pc);

    LOG_DEBUG("BL to " + hex32(new_pc));
    } else {
        // Regular branch (B or B<cond>)
        int32_t offset = static_cast<int32_t>(fields.imm);
        new_pc = current_pc + 4 + offset;
        
        m_registers->set_pc(new_pc);
        LOG_DEBUG("Branch taken to " + hex32(new_pc));
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
        // Immediate operand (Format 3: MOVS/CMP/ADDS/SUBS)
        if ((fields.opcode & 0xE000) == 0x2000) {
            uint32_t rn_val = m_registers->read_register(fields.rn);
            switch (fields.alu_op) {
                case 0: // MOVS
                    m_registers->write_register(fields.rd, fields.imm);
                    if (fields.s_bit) update_flags(fields.imm, false, false);
                    return false;
                case 1: { // CMP
                    uint32_t res = rn_val - fields.imm;
                    bool c = (rn_val >= fields.imm);
                    bool v = ((rn_val ^ fields.imm) & (rn_val ^ res)) >> 31;
                    update_flags(res, c, v);
                    return false;
                }
                case 2: { // ADD
                    uint32_t res = rn_val + fields.imm;
                    bool c = (res < rn_val);
                    bool v = ((rn_val ^ res) & (fields.imm ^ res)) >> 31;
                    m_registers->write_register(fields.rd, res);
                    if (fields.s_bit) update_flags(res, c, v);
                    return false;
                }
                case 3: { // SUB
                    uint32_t res = rn_val - fields.imm;
                    bool c = (rn_val >= fields.imm);
                    bool v = ((rn_val ^ fields.imm) & (rn_val ^ res)) >> 31;
                    m_registers->write_register(fields.rd, res);
                    if (fields.s_bit) update_flags(res, c, v);
                    return false;
                }
            }
        }
        // Fallback: treat as op with immediate
        op1 = m_registers->read_register(fields.rn);
        op2 = fields.imm;
    } else if ((fields.opcode & 0xFC00) == 0x4400) {
        // Hi register operations (Format 5): ADD/CMP/MOV with high regs
        uint32_t rd_val = m_registers->read_register(fields.rd);
        uint32_t rm_val = m_registers->read_register(fields.rm);
        switch (fields.alu_op & 0x3) {
            case 0: { // ADD (hi)
                uint32_t res = rd_val + rm_val;
                m_registers->write_register(fields.rd, res);
                // Flags are not affected by ADD (hi)
                return false;
            }
            case 1: { // CMP (hi)
                uint32_t res = rd_val - rm_val;
                bool c = (rd_val >= rm_val);
                bool v = ((rd_val ^ rm_val) & (rd_val ^ res)) >> 31;
                update_flags(res, c, v);
                return false;
            }
            case 2: { // MOV (hi)
                m_registers->write_register(fields.rd, rm_val);
                // Flags not affected
                return false;
            }
            case 3: {
                // BX/BLX handled in execute_branch via instruction type
                return false;
            }
        }
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

    // Handle ADD/SUB SP, #imm7 special (Format 13)
    if (fields.type == INST_T16_ADD_SP_IMM7 || fields.type == INST_T16_SUB_SP_IMM7) {
        uint32_t sp = m_registers->read_register(13);
        uint32_t res = (fields.type == INST_T16_ADD_SP_IMM7) ? (sp + fields.imm) : (sp - fields.imm);
        m_registers->write_register(13, res);
        LOG_DEBUG(std::string((fields.type == INST_T16_ADD_SP_IMM7) ? "ADD SP, #" : "SUB SP, #") + std::to_string(fields.imm) + " -> SP=" + hex32(res));
        return false;
    }
    
    // Determine operation based on ALU op code
    switch (fields.alu_op) {
        case 0: // AND (Format 4)
            result = op1 & op2;
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
    if (fields.rm != 0xFF) {
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
    LOG_DEBUG("Load: R" + std::to_string(fields.rd) + " = [" + hex32(address) + "] = " + hex32(data));
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
        
    LOG_DEBUG("Store: [" + hex32(address) + "] = R" + std::to_string(fields.rd) + " = " + hex32(data));
    write_memory(address, data, size, data_bus);
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
                        // POP PC - this can be a normal return or an EXC_RETURN sequence
                        LOG_DEBUG("POP PC: " + hex32(data));

                        // Detect EXC_RETURN magic values
                        if (m_cpu && (data == 0xFFFFFFF1u || data == 0xFFFFFFF9u || data == 0xFFFFFFFDu)) {
                            // Advance SP past the stacked PC first
                            address += 4;
                            m_registers->write_register(13, address); // Update SP
                            if (m_cpu->try_exception_return(data)) {
                                return true; // PC changed by exception return
                            }
                            // If exception return failed, fall through to normal branch
                        } else {
                            // Normal POP to PC
                            m_registers->set_pc(data & ~1u); // Clear Thumb bit
                            address += 4;
                            m_registers->write_register(13, address); // Update SP
                            return true; // PC changed
                        }
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
    // Handle Load address (Format 12): ADD Rd, PC/SP, #imm
    if ((fields.opcode & 0xF000) == 0xA000) {
        uint32_t op1, op2, result;
        if (fields.rn == 15) {
            // PC-relative: align (PC+4) to word boundary per ARMv6-M
            op1 = (m_registers->get_pc() + 4) & ~3u;
        } else {
            // SP-relative
            op1 = m_registers->read_register(fields.rn);
        }
        op2 = fields.imm;
        result = op1 + op2;
        m_registers->write_register(fields.rd, result);
        LOG_DEBUG("ADR address: " + hex32(result));
    }

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
        LOG_DEBUG("SP adjusted: SP = " + hex32(result));
    }

    return false;
}

bool Execute::execute_exception(const InstructionFields& fields)
{
    // Trigger SVC exception (synchronous)
    LOG_INFO("SVC instruction executed, requesting SVCall exception");
    if (m_cpu) {
        // Ask CPU to raise SVC so it will stack and vector on next check
        // We can't include CPU.h in this TU (forward-declared), so call via a simple method
        // The CPU will prioritize SVC appropriately.
        // Note: No immediate PC change here; CPU's exception logic will handle stacking and branching.
        m_cpu->request_svc();
    } else {
        LOG_WARNING("CPU not connected to Execute; SVC ignored");
    }
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
    auto* bus = static_cast<tlm_utils::simple_initiator_socket<CPU>*>(socket);

    // DMI fast path (cache retained across calls)
    if (m_data_dmi_valid && address >= m_data_dmi.get_start_address() && (address + size - 1) <= m_data_dmi.get_end_address()) {
        auto base = m_data_dmi.get_dmi_ptr();
        uint64_t off = static_cast<uint64_t>(address) - static_cast<uint64_t>(m_data_dmi.get_start_address());
        uint32_t val = 0;
        std::memcpy(&val, base + off, size);
        wait(m_data_dmi.get_read_latency());
        Performance::getInstance().increment_memory_reads();
        if (Log::getInstance().get_log_level() >= LOG_TRACE) {
            Log::getInstance().log_memory_access(address, val, size, false);
        }
        return val;
    }

    // Try to acquire new DMI
    tlm_generic_payload dmi_req;
    tlm_dmi dmi_data;
    dmi_req.set_command(TLM_READ_COMMAND);
    dmi_req.set_address(address);
    if ((*bus)->get_direct_mem_ptr(dmi_req, dmi_data)) {
        m_data_dmi = dmi_data;
        m_data_dmi_valid = true;
        if (address >= m_data_dmi.get_start_address() && (address + size - 1) <= m_data_dmi.get_end_address()) {
            auto base = m_data_dmi.get_dmi_ptr();
            uint64_t off = static_cast<uint64_t>(address) - static_cast<uint64_t>(m_data_dmi.get_start_address());
            uint32_t val = 0;
            std::memcpy(&val, base + off, size);
            wait(m_data_dmi.get_read_latency());
            Performance::getInstance().increment_memory_reads();
            if (Log::getInstance().get_log_level() >= LOG_TRACE) {
                Log::getInstance().log_memory_access(address, val, size, false);
            }
            return val;
        }
    }

    // Fallback to TLM b_transport
    tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    uint32_t data = 0;
    trans.set_command(TLM_READ_COMMAND);
    trans.set_address(address);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(size);
    trans.set_streaming_width(size);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(true);
    trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
    (*bus)->b_transport(trans, delay);
    if (trans.get_response_status() != TLM_OK_RESPONSE) {
        LOG_ERROR("Data read failed at address " + hex32(address));
        return 0;
    }
    wait(delay);
    Performance::getInstance().increment_memory_reads();
    if (Log::getInstance().get_log_level() >= LOG_TRACE) {
        Log::getInstance().log_memory_access(address, data, size, false);
    }
    return data;
}

void Execute::write_memory(uint32_t address, uint32_t data, uint32_t size, void* socket)
{
    auto* bus = static_cast<tlm_utils::simple_initiator_socket<CPU>*>(socket);

    // DMI fast path for writes
    if (m_data_dmi_valid && address >= m_data_dmi.get_start_address() && (address + size - 1) <= m_data_dmi.get_end_address() && m_data_dmi.is_write_allowed()) {
        auto base = m_data_dmi.get_dmi_ptr();
        uint64_t off = static_cast<uint64_t>(address) - static_cast<uint64_t>(m_data_dmi.get_start_address());
        std::memcpy(base + off, &data, size);
        wait(m_data_dmi.get_write_latency());
        Performance::getInstance().increment_memory_writes();
        if (Log::getInstance().get_log_level() >= LOG_TRACE) {
            Log::getInstance().log_memory_access(address, data, size, true);
        }
        return;
    }

    // Try to acquire DMI
    tlm_generic_payload dmi_req;
    tlm_dmi dmi_data;
    dmi_req.set_command(TLM_WRITE_COMMAND);
    dmi_req.set_address(address);
    if ((*bus)->get_direct_mem_ptr(dmi_req, dmi_data)) {
        m_data_dmi = dmi_data;
        m_data_dmi_valid = true;
        if (address >= m_data_dmi.get_start_address() && (address + size - 1) <= m_data_dmi.get_end_address() && m_data_dmi.is_write_allowed()) {
            auto base = m_data_dmi.get_dmi_ptr();
            uint64_t off = static_cast<uint64_t>(address) - static_cast<uint64_t>(m_data_dmi.get_start_address());
            std::memcpy(base + off, &data, size);
            wait(m_data_dmi.get_write_latency());
            Performance::getInstance().increment_memory_writes();
            if (Log::getInstance().get_log_level() >= LOG_TRACE) {
                Log::getInstance().log_memory_access(address, data, size, true);
            }
            return;
        }
    }

    // Fallback to TLM
    tlm_generic_payload trans;
    sc_time delay = SC_ZERO_TIME;
    trans.set_command(TLM_WRITE_COMMAND);
    trans.set_address(address);
    trans.set_data_ptr(reinterpret_cast<unsigned char*>(&data));
    trans.set_data_length(size);
    trans.set_streaming_width(size);
    trans.set_byte_enable_ptr(nullptr);
    trans.set_dmi_allowed(true);
    trans.set_response_status(TLM_INCOMPLETE_RESPONSE);
    (*bus)->b_transport(trans, delay);
    if (trans.get_response_status() != TLM_OK_RESPONSE) {
        LOG_ERROR("Data write failed at address " + hex32(address));
        return;
    }
    wait(delay);
    Performance::getInstance().increment_memory_writes();
    if (Log::getInstance().get_log_level() >= LOG_TRACE) {
        Log::getInstance().log_memory_access(address, data, size, true);
    }
}

bool Execute::execute_extend(const InstructionFields& fields)
{
    uint32_t rm_val = m_registers->read_register(fields.rm);
    uint32_t result = 0;
    
    switch (fields.alu_op) {
        case 0: // SXTH - Sign extend halfword (16-bit) to word (32-bit)
            result = static_cast<int32_t>(static_cast<int16_t>(rm_val & 0xFFFF));
            break;
        case 1: // SXTB - Sign extend byte (8-bit) to word (32-bit) 
            result = static_cast<int32_t>(static_cast<int8_t>(rm_val & 0xFF));
            break;
        case 2: // UXTH - Zero extend halfword (16-bit) to word (32-bit)
            result = rm_val & 0xFFFF;
            break;
        case 3: // UXTB - Zero extend byte (8-bit) to word (32-bit)
            result = rm_val & 0xFF;
            break;
    }
    
    m_registers->write_register(fields.rd, result);
    LOG_DEBUG("Extend: R" + std::to_string(fields.rd) + " = extend(R" + std::to_string(fields.rm) + 
              ") = " + hex32(result));
    
    return false;
}

bool Execute::execute_memory_barrier(const InstructionFields& fields)
{
    // Memory barriers are typically no-ops in a single-core simulation
    // but we implement them for completeness and debugging
    
    std::string barrier_type;
    switch (fields.type) {
        case INST_T32_DSB:
            barrier_type = "DSB";
            break;
        case INST_T32_DMB:
            barrier_type = "DMB";
            break;
        case INST_T32_ISB:
            barrier_type = "ISB";
            break;
        default:
            barrier_type = "Unknown";
            break;
    }
    
    std::string option_str;
    switch (fields.imm) {
        case 15: option_str = "SY"; break;  // System
        case 14: option_str = "ST"; break;  // Store
        case 11: option_str = "ISH"; break; // Inner Shareable
        case 10: option_str = "ISHST"; break; // Inner Shareable Store
        case 7:  option_str = "NSH"; break; // Non-shareable
        case 6:  option_str = "NSHST"; break; // Non-shareable Store
        case 3:  option_str = "OSH"; break; // Outer Shareable
        case 2:  option_str = "OSHST"; break; // Outer Shareable Store
        default: option_str = "#" + std::to_string(fields.imm); break;
    }
    
    LOG_DEBUG(barrier_type + " " + option_str + " - Memory barrier executed");
    
    // In a real implementation, this would ensure memory ordering
    // For simulation purposes, we just acknowledge the instruction
    return false;
}

bool Execute::execute_cps(const InstructionFields& fields)
{
    // CPS (Change Processor State) instruction
    // For ARMv6-M, this only affects the PRIMASK register (interrupt masking)
    
    std::string operation = (fields.alu_op == 1) ? "CPSID" : "CPSIE";
    
    if (fields.imm & 0x1) { // I bit - affects PRIMASK
        if (fields.alu_op == 1) {
            // CPSID I - disable interrupts (set PRIMASK = 1)
            m_registers->disable_interrupts();
            LOG_DEBUG("CPSID I - Interrupts disabled (PRIMASK = 1)");
        } else {
            // CPSIE I - enable interrupts (set PRIMASK = 0) 
            m_registers->enable_interrupts();
            LOG_DEBUG("CPSIE I - Interrupts enabled (PRIMASK = 0)");
        }
        
        // Log the current PRIMASK value for confirmation
        uint32_t primask = m_registers->get_primask();
        LOG_DEBUG(operation + " I completed - PRIMASK = " + std::to_string(primask));
    }
    
    // F bit (fields.imm & 0x2) would affect FAULTMASK, but ARMv6-M doesn't support this
    // A bit (fields.imm & 0x4) would affect CONTROL register, but not typically used with CPS
    
    return false;
}

bool Execute::execute_msr(const InstructionFields& fields)
{
    // MSR (Move to Special Register) instruction
    uint32_t source_value = m_registers->read_register(fields.rn);
    uint32_t spec_reg = fields.imm;
    
    std::string reg_name;
    switch (spec_reg) {
        case 0x09: // PSP (Process Stack Pointer)
            reg_name = "PSP";
            m_registers->set_psp(source_value);
            LOG_DEBUG("MSR PSP, R" + std::to_string(fields.rn) + " = " + hex32(source_value));
            break;
            
        case 0x14: // CONTROL 
            reg_name = "CONTROL";
            m_registers->set_control(source_value);
            // Bit 0: Thread mode privilege level (0=privileged, 1=unprivileged)  
            // Bit 1: Thread mode stack selection (0=MSP, 1=PSP)
            LOG_DEBUG("MSR CONTROL, R" + std::to_string(fields.rn) + " = " + hex32(source_value) + 
                     " (SPSEL=" + std::to_string((source_value >> 1) & 1) + 
                     ", nPRIV=" + std::to_string(source_value & 1) + ")");
            break;
            
        case 0x00: // APSR (Application Program Status Register)
            {
                reg_name = "APSR";
                // Only allow updating of condition flags (bits 31-28)
                uint32_t current_psr = m_registers->get_psr();
                uint32_t new_psr = (current_psr & ~0xF0000000) | (source_value & 0xF0000000);
                m_registers->set_psr(new_psr);
                LOG_DEBUG("MSR APSR, R" + std::to_string(fields.rn) + " = " + hex32(source_value) +
                         " (flags only)");
                break;
            }
            
        case 0x10: // PRIMASK
            {
                reg_name = "PRIMASK";
                m_registers->set_primask(source_value);
                bool interrupts_enabled = m_registers->interrupts_enabled();
                LOG_DEBUG("MSR PRIMASK, R" + std::to_string(fields.rn) + " = " + hex32(source_value) +
                         " (interrupts " + (interrupts_enabled ? "enabled" : "disabled") + ")");
                break;
            }
        case 0x08: // MSP
            {
                reg_name = "MSP";
                m_registers->set_msp(source_value);
                LOG_DEBUG("MSR MSP, R" + std::to_string(fields.rn) + " = " + hex32(source_value));
                break;
            }
        default:
            reg_name = "UNKNOWN(" + std::to_string(spec_reg) + ")";
            LOG_WARNING("MSR to unknown special register: " + std::to_string(spec_reg));
            break;
    }
    
    return false;
}

bool Execute::execute_mrs(const InstructionFields& fields)
{
    // MRS (Move from Special Register) instruction
    uint32_t spec_reg = fields.imm;
    uint32_t value = 0;
    std::string reg_name;
    
    switch (spec_reg) {
        case 0x09: // PSP (Process Stack Pointer)
            reg_name = "PSP";
            value = m_registers->get_psp();
            break;
            
        case 0x14: // CONTROL
            reg_name = "CONTROL";
            value = m_registers->get_control();
            break;
            
        case 0x00: // APSR (Application Program Status Register)
            reg_name = "APSR";
            value = m_registers->get_psr() & 0xF0000000; // Only condition flags
            break;
            
        case 0x10: // PRIMASK
            reg_name = "PRIMASK";
            value = m_registers->get_primask();
            break;
            
        case 0x08: // MSP (Main Stack Pointer)
            reg_name = "MSP";
            value = m_registers->get_msp();
            break;
            
        default:
            reg_name = "UNKNOWN(" + std::to_string(spec_reg) + ")";
            LOG_WARNING("MRS from unknown special register: " + std::to_string(spec_reg));
            value = 0;
            break;
    }
    
    m_registers->write_register(fields.rd, value);
    LOG_DEBUG("MRS R" + std::to_string(fields.rd) + ", " + reg_name + " = " + hex32(value));
    
    return false;
}