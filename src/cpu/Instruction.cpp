#include "Instruction.h"
#include "Log.h"
#include <sstream>

Instruction::Instruction(sc_module_name name) : sc_module(name)
{
    LOG_INFO("Instruction decoder initialized");
}

InstructionFields Instruction::decode(uint16_t instruction)
{
    // Initialize all fields to known values
    InstructionFields fields = {};
    fields.opcode = instruction;
    fields.rd = 0xFF;   // Initialize to invalid register
    fields.rn = 0xFF;   // Initialize to invalid register
    fields.rm = 0xFF;   // Initialize to invalid register
    fields.rs = 0xFF;   // Initialize to invalid register (not used marker)
    fields.imm = 0;
    fields.cond = 0;
    fields.s_bit = false;
    fields.shift_type = 0;
    fields.shift_amount = 0;
    fields.alu_op = 0;
    fields.h1 = false;
    fields.h2 = false;
    fields.reg_list = 0;
    fields.load_store_bit = false;
    fields.byte_word = 0;
    fields.type = identify_instruction_type(instruction);
    
    // Decode based on instruction type
    switch (fields.type) {
        case INST_BRANCH:
            fields = decode_branch(instruction);
            break;
        case INST_DATA_PROCESSING:
            fields = decode_data_processing(instruction);
            break;
        case INST_LOAD_STORE:
            fields = decode_load_store(instruction);
            break;
        case INST_LOAD_STORE_MULTIPLE:
            fields = decode_load_store_multiple(instruction);
            break;
        case INST_STATUS_REGISTER:
            fields = decode_status_register(instruction);
            break;
        case INST_MISCELLANEOUS:
            fields = decode_miscellaneous(instruction);
            break;
        default:
            fields.type = INST_UNKNOWN;
            break;
    }
    
    // Debug logging for decode verification
    if (Log::getInstance().get_log_level() >= LOG_DEBUG) {
        std::stringstream ss;
        ss << "DECODE: opcode=0x" << std::hex << fields.opcode;
        if (fields.rd != 0xFF) ss << " rd=" << std::dec << (int)fields.rd;
        if (fields.rn != 0xFF) ss << " rn=" << std::dec << (int)fields.rn;
        if (fields.rm != 0xFF) ss << " rm=" << std::dec << (int)fields.rm;
        if (fields.rs != 0xFF) ss << " rs=" << std::dec << (int)fields.rs;
        if (fields.imm != 0) ss << " imm=0x" << std::hex << fields.imm;
        ss << " type=" << std::dec << fields.type;
        LOG_DEBUG("INSTRUCTION DECODE: " + ss.str());
    }
    
    return fields;
}

InstructionType Instruction::identify_instruction_type(uint16_t instruction)
{
    // ARMv6-M Thumb instruction identification based on bits 15:10 encoding patterns
    // According to TRM documentation, opcode is in bits 15:10 (6 bits)
    uint16_t opcode_field = (instruction >> 10) & 0x3F; // Extract bits 15:10
    
    // Format 1: Move shifted register (000000 - 000101, excluding 000110-000111)
    if ((opcode_field & 0x38) == 0x00) {
        if ((opcode_field & 0x06) != 0x06) { // Exclude 000110 and 000111
            return INST_MOVE_SHIFTED_REG;
        }
    }
    
    // Format 2: Add/subtract (000110, 000111) 
    if ((opcode_field & 0x3E) == 0x06) {
        return INST_ADD_SUB_REG_IMM;
    }
    
    // Format 3: Move/compare/add/subtract immediate (001000 - 001011)
    if ((opcode_field & 0x3C) == 0x08) {
        return INST_MOV_CMP_ADD_SUB_IMM;
    }
    
    // Format 4: ALU operations (010000)
    if (opcode_field == 0x10) {
        return INST_ALU_OPERATIONS;
    }
    
    // Format 5: Hi register operations/branch exchange (010001)
    if (opcode_field == 0x11) {
        return INST_HI_REG_BX;
    }
    
    // Format 6: PC-relative load (01001x)
    if ((opcode_field & 0x3E) == 0x12) {
        return INST_LOAD_STORE_REG_OFF; // PC-relative is a special case of load/store
    }
    
    // Format 7: Load/store with register offset + sign-extended (0101xx)
    if ((opcode_field & 0x3C) == 0x14) {
        // Check specific patterns for sign-extended loads using bits 11:9
        uint16_t bits_11_9 = (instruction >> 9) & 0x07; // bits 11:9
        if (bits_11_9 == 0x3 || bits_11_9 == 0x7) { // 011 and 111 patterns are sign-extended
            return INST_LOAD_STORE_SIGN_EXT; 
        }
        return INST_LOAD_STORE_REG_OFF;
    }
    
    // Format 9: Load/store with immediate offset (011xxx)  
    if ((opcode_field & 0x38) == 0x18) {
        return INST_LOAD_STORE_IMM_OFF;
    }
    
    // Format 8: Load/store halfword (1000xx)
    if ((opcode_field & 0x3C) == 0x20) {
        return INST_LOAD_STORE_HALFWORD;
    }
    
    // Format 11: SP-relative load/store (1001xx)
    if ((opcode_field & 0x3C) == 0x24) {
        return INST_LOAD_STORE_SP_REL;
    }
    
    // Format 12: Load address (1010xx)
    if ((opcode_field & 0x3C) == 0x28) {
        return INST_LOAD_ADDRESS;
    }
    
    // Format 13: Add offset to Stack Pointer and PUSH/POP (1011xxxx)
    if ((opcode_field & 0x3C) == 0x2C) {
        // Check for PUSH/POP: bits 11:9 should be x1xx (bit 9 = 1)
        if ((instruction & 0x0600) == 0x0400) {
            return INST_PUSH_POP;
        }
        return INST_ADD_SP_IMM;
    }
    
    // Format 15: Multiple load/store (1100xx)
    if ((opcode_field & 0x3C) == 0x30) {
        return INST_LOAD_STORE_MULTIPLE;
    }
    
    // Format 16: Conditional branch (1101xxxx, but not 11011111)
    if ((opcode_field & 0x3C) == 0x34) {
        // Format 17: Software interrupt (11011111)
        if ((instruction & 0xFF00) == 0xDF00) {
            return INST_SWI;
        }
        return INST_BRANCH_COND;
    }
    
    // Format 18: Unconditional branch (11100xxx)
    if ((opcode_field & 0x38) == 0x38 && (opcode_field & 0x04) == 0x00) {
        return INST_BRANCH_UNCOND;
    }
    
    // Format 19: Long branch with link (1111xxxx)
    if ((opcode_field & 0x3C) == 0x3C) {
        return INST_BRANCH_LINK;
    }
    
    return INST_UNKNOWN;
}

InstructionFields Instruction::decode_branch(uint16_t instruction)
{
    InstructionFields fields = {};
    fields.opcode = instruction;
    fields.type = INST_BRANCH;
    
    if ((instruction & 0xF000) == 0xD000) {
        // Conditional branch (1101xxxx)
        fields.cond = (instruction >> 8) & 0xF; // Bits 11:8
        fields.imm = instruction & 0xFF;        // Bits 7:0
        
        // Sign extend 8-bit offset to 16-bit
        if (fields.imm & 0x80) {
            fields.imm |= 0xFF00;
        }
        fields.imm *= 2; // Convert to byte offset
    } else if ((instruction & 0xF800) == 0xE000) {
        // Unconditional branch (11100xxx)
        fields.cond = 0xE;  // Always condition
        fields.imm = instruction & 0x7FF; // Bits 10:0
        
        // Sign extend 11-bit offset
        if (fields.imm & 0x400) {
            fields.imm |= 0xF800;
        }
        fields.imm *= 2; // Convert to byte offset
    } else if ((instruction & 0xF000) == 0xF000) {
        // Long branch with link - first half (1111xxxx)
        // This is a 32-bit instruction, we need the second half too
        fields.cond = 0xE; // Always condition
        fields.imm = instruction & 0x7FF; // High part of offset
        fields.alu_op = 1; // Mark as BL instruction
    }
    
    return fields;
}

InstructionFields Instruction::decode_data_processing(uint16_t instruction)
{
    InstructionFields fields = {};
    fields.opcode = instruction;
    fields.type = INST_DATA_PROCESSING;
    
    // Identify specific data processing format
    if ((instruction & 0xE000) == 0x0000) {
        // Format 1: Move shifted register (000xx)
        if ((instruction & 0x1800) != 0x1800) {
            fields.rd = instruction & 0x7;                    // Bits 2:0
            fields.rm = (instruction >> 3) & 0x7;            // Bits 5:3
            fields.shift_amount = (instruction >> 6) & 0x1F; // Bits 10:6
            fields.shift_type = (instruction >> 11) & 0x3;   // Bits 12:11
        } else {
            // Format 2: Add/subtract (00110, 00111)
            fields.rd = instruction & 0x7;           // Bits 2:0
            fields.rn = (instruction >> 3) & 0x7;   // Bits 5:3
            bool immediate_flag = (instruction & 0x0400) != 0; // Bit 10
            if (immediate_flag) {
                fields.imm = (instruction >> 6) & 0x7; // 3-bit immediate
                fields.rm = 0xFF; // Mark as immediate
            } else {
                fields.rm = (instruction >> 6) & 0x7;  // Register
            }
            fields.alu_op = (instruction & 0x0200) ? 2 : 1; // Bit 9: 0=ADD, 1=SUB
        }
    } else if ((instruction & 0xE000) == 0x2000) {
        // Format 3: Move/compare/add/subtract immediate (001xx)
        fields.rd = (instruction >> 8) & 0x7;    // Bits 10:8
        fields.rn = fields.rd;                   // Source same as dest for most ops
        fields.imm = instruction & 0xFF;         // Bits 7:0
        fields.alu_op = (instruction >> 11) & 0x3; // Bits 12:11
    } else if ((instruction & 0xFC00) == 0x4000) {
        // Format 4: ALU operations (010000xxxx)
        fields.rd = instruction & 0x7;           // Bits 2:0 (also source)
        fields.rn = fields.rd;                   // Source same as dest
        fields.rm = (instruction >> 3) & 0x7;   // Bits 5:3
        fields.alu_op = (instruction >> 6) & 0xF; // Bits 9:6
    } else if ((instruction & 0xFC00) == 0x4400) {
        // Format 5: Hi register operations (010001xxxx)
        fields.rd = instruction & 0x7;           // Bits 2:0
        fields.rm = (instruction >> 3) & 0x7;   // Bits 5:3
        fields.h1 = (instruction & 0x80) != 0;  // Bit 7
        fields.h2 = (instruction & 0x40) != 0;  // Bit 6
        fields.alu_op = (instruction >> 8) & 0x3; // Bits 9:8
        // Adjust for Hi registers
        if (fields.h1) fields.rd += 8;
        if (fields.h2) fields.rm += 8;
    } else if ((instruction & 0xF000) == 0xA000) {
        // Format 12: Load address (1010x)
        fields.rd = (instruction >> 8) & 0x7;    // Bits 10:8
        fields.imm = (instruction & 0xFF) * 4;   // Bits 7:0, word-aligned
        fields.rn = (instruction & 0x0800) ? 13 : 15; // Bit 11: SP(13) or PC(15)
        fields.alu_op = 1; // ADD operation
    } else {
        // Basic decode for other cases
        fields.rd = instruction & 0x7;
        fields.rn = (instruction >> 3) & 0x7;
        fields.rm = (instruction >> 6) & 0x7;
    }
    
    return fields;
}

InstructionFields Instruction::decode_load_store(uint16_t instruction)
{
    InstructionFields fields = {};
    fields.opcode = instruction;
    fields.type = INST_LOAD_STORE;
    
    if ((instruction & 0xF800) == 0x4800) {
        // PC-relative load (01001)
        fields.rd = (instruction >> 8) & 0x7;    // Bits 10:8
        fields.imm = (instruction & 0xFF) * 4;   // Bits 7:0, word-aligned
        fields.rn = 15; // PC register
        fields.load_store_bit = true; // Always load
        fields.byte_word = false;     // Always word
    } else if ((instruction & 0xF000) == 0x5000) {
        // Load/store with register offset (0101x)
        fields.rd = instruction & 0x7;           // Bits 2:0
        fields.rn = (instruction >> 3) & 0x7;   // Bits 5:3 (base register)
        fields.rm = (instruction >> 6) & 0x7;   // Bits 8:6 (offset register)
        fields.load_store_bit = (instruction & 0x0800) != 0; // Bit 11 (L bit)
        fields.byte_word = (instruction & 0x0400) != 0;      // Bit 10 (B bit)
        
        // Check for sign-extended loads (0101xx1x)
        if ((instruction & 0x0200) != 0) {
            // Sign-extended byte/halfword operations
            uint8_t op = (instruction >> 10) & 0x3;
            fields.alu_op = op; // 0=STRH, 1=LDSB, 2=LDRH, 3=LDSH
        }
    } else if ((instruction & 0xE000) == 0x6000) {
        // Load/store with immediate offset (011xx)
        fields.rd = instruction & 0x7;           // Bits 2:0
        fields.rn = (instruction >> 3) & 0x7;   // Bits 5:3 (base register)
        fields.imm = (instruction >> 6) & 0x1F; // Bits 10:6 (immediate offset)
        fields.load_store_bit = (instruction & 0x0800) != 0; // Bit 11 (L bit)
        fields.byte_word = (instruction & 0x1000) != 0;      // Bit 12 (B bit)
        
        // Scale immediate for word access
        if (!fields.byte_word) {
            fields.imm *= 4;
        }
    } else if ((instruction & 0xF000) == 0x8000) {
        // Load/store halfword (1000x)
        fields.rd = instruction & 0x7;           // Bits 2:0
        fields.rn = (instruction >> 3) & 0x7;   // Bits 5:3 (base register)
        fields.imm = ((instruction >> 6) & 0x1F) * 2; // Bits 10:6, halfword-aligned
        fields.load_store_bit = (instruction & 0x0800) != 0; // Bit 11 (L bit)
        fields.byte_word = 2; // Halfword indicator
    } else if ((instruction & 0xF000) == 0x9000) {
        // SP-relative load/store (1001x)
        fields.rd = (instruction >> 8) & 0x7;    // Bits 10:8
        fields.imm = (instruction & 0xFF) * 4;   // Bits 7:0, word-aligned
        fields.rn = 13; // SP register
        fields.load_store_bit = (instruction & 0x0800) != 0; // Bit 11 (L bit)
        fields.byte_word = false; // Always word
    } else {
        // Basic decode for compatibility
        fields.rd = instruction & 0x7;
        fields.rn = (instruction >> 3) & 0x7;
        fields.imm = (instruction >> 6) & 0x1F;
        fields.load_store_bit = (instruction & 0x0800) != 0;
    }
    
    return fields;
}

InstructionFields Instruction::decode_load_store_multiple(uint16_t instruction)
{
    InstructionFields fields = {};
    fields.opcode = instruction;
    fields.type = INST_LOAD_STORE_MULTIPLE;
    
    if ((instruction & 0xF600) == 0xB400) {
        // PUSH/POP registers (1011x10x)
        fields.reg_list = instruction & 0xFF;   // Bits 7:0 (register list)
        fields.load_store_bit = (instruction & 0x0800) != 0; // Bit 11 (L bit: 0=PUSH, 1=POP)
        fields.rn = 13; // SP register (implicit)
        
        // R bit (bit 8) indicates LR/PC inclusion
        bool r_bit = (instruction & 0x0100) != 0;
        if (r_bit) {
            if (fields.load_store_bit) {
                fields.reg_list |= 0x8000; // Include PC in POP
            } else {
                fields.reg_list |= 0x4000; // Include LR in PUSH
            }
        }
    } else {
        // Regular LDM/STM (1100x)
        fields.rn = (instruction >> 8) & 0x7;   // Bits 10:8 (base register)
        fields.reg_list = instruction & 0xFF;   // Bits 7:0 (register list)
        fields.load_store_bit = (instruction & 0x0800) != 0; // Bit 11 (L bit)
    }
    
    return fields;
}

InstructionFields Instruction::decode_status_register(uint16_t instruction)
{
    InstructionFields fields = {};
    fields.opcode = instruction;
    fields.type = INST_STATUS_REGISTER;
    
    return fields;
}

InstructionFields Instruction::decode_miscellaneous(uint16_t instruction)
{
    InstructionFields fields = {};
    fields.opcode = instruction;
    fields.type = INST_MISCELLANEOUS;
    
    if ((instruction & 0xFF00) == 0xB000) {
        // Add offset to Stack Pointer (10110000)
        fields.imm = (instruction & 0x7F) * 4;   // Bits 6:0, word-aligned
        fields.alu_op = (instruction & 0x80) ? 2 : 1; // Bit 7: 0=ADD, 1=SUB
        fields.rd = 13; // SP register
        fields.rn = 13; // SP register
    }
    
    return fields;
}

bool Instruction::is_32bit_instruction(uint16_t first_half)
{
    // Check for 32-bit Thumb-2 instructions
    return ((first_half & 0xE000) == 0xE000) && ((first_half & 0x1800) != 0x0000);
}

const char* Instruction::get_instruction_name(const InstructionFields& fields)
{
    switch (fields.type) {
        case INST_BRANCH:
            return (fields.cond == 0xE) ? "B" : "B<cond>";
        case INST_DATA_PROCESSING:
            return "ALU";
        case INST_LOAD_STORE:
            return "LDR/STR";
        case INST_LOAD_STORE_MULTIPLE:
            return "LDM/STM";
        case INST_STATUS_REGISTER:
            return "MSR/MRS";
        case INST_MISCELLANEOUS:
            return "MISC";
        case INST_EXCEPTION:
            return "SVC";
        default:
            return "UNKNOWN";
    }
}