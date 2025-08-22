#include "Instruction.h"
#include "Log.h"
#include <sstream>

Instruction::Instruction(sc_module_name name) : sc_module(name)
{
    LOG_INFO("Instruction decoder initialized");
}

InstructionFields Instruction::decode(uint32_t instruction, bool is_32bit)
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
    fields.is_32bit = is_32bit;
    
    // For 16-bit instructions, use only the lower 16 bits for type identification
    uint16_t first_half = is_32bit ? (instruction >> 16) & 0xFFFF : instruction & 0xFFFF;
    fields.type = identify_instruction_type(first_half);
    
    if (is_32bit) {
        // Handle 32-bit Thumb-2 instructions
        fields = decode_32bit_instruction(instruction);
    } else {
        // Handle 16-bit Thumb instructions (existing logic)
        switch (fields.type) {
            case INST_BRANCH:
                fields = decode_branch(first_half);
                break;
            case INST_DATA_PROCESSING:
                fields = decode_data_processing(first_half);
                break;
            case INST_LOAD_STORE:
                fields = decode_load_store(first_half);
                break;
            case INST_LOAD_STORE_MULTIPLE:
                fields = decode_load_store_multiple(first_half);
                break;
            case INST_STATUS_REGISTER:
                fields = decode_status_register(first_half);
                break;
            case INST_MISCELLANEOUS:
                fields = decode_miscellaneous(first_half);
                break;
            default:
                fields.type = INST_UNKNOWN;
                break;
        }
    }
    
    // Set the 32-bit flag in all cases
    fields.is_32bit = is_32bit;
    
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
        ss << " 32bit=" << fields.is_32bit;
        LOG_DEBUG("INSTRUCTION DECODE: " + ss.str());
    }
    
    return fields;
}

InstructionType Instruction::identify_instruction_type(uint16_t instruction)
{
    // ARMv6-M Thumb instruction identification based on encoding patterns
    
    // Format 1-2: Move shifted register and Add/subtract (000xx - 001xx)
    if ((instruction & 0xE000) == 0x0000) {
        if ((instruction & 0x1800) == 0x1800) {
            return INST_DATA_PROCESSING; // Add/subtract format
        }
        return INST_DATA_PROCESSING; // Move shifted register format
    }
    
    // Format 3: Move/compare/add/subtract immediate (001xx)
    if ((instruction & 0xE000) == 0x2000) {
        return INST_DATA_PROCESSING;
    }
    
    // Format 4-5: ALU operations and Hi register operations (010xx)
    if ((instruction & 0xF000) == 0x4000) {
        if ((instruction & 0x0400) == 0x0000) {
            return INST_DATA_PROCESSING; // ALU operations
        } else {
            if ((instruction & 0x0300) == 0x0300) {
                return INST_BRANCH; // BX/BLX instructions
            }
            return INST_DATA_PROCESSING; // Hi register operations
        }
    }
    
    // PC-relative load (01001)
    if ((instruction & 0xF800) == 0x4800) {
        return INST_LOAD_STORE;
    }
    
    // Load/store register offset and sign-extended (0101x)
    if ((instruction & 0xF000) == 0x5000) {
        return INST_LOAD_STORE;
    }
    
    // Load/store immediate offset (011xx)
    if ((instruction & 0xE000) == 0x6000) {
        return INST_LOAD_STORE;
    }
    
    // Load/store halfword (1000x)
    if ((instruction & 0xF000) == 0x8000) {
        return INST_LOAD_STORE;
    }
    
    // SP-relative load/store (1001x)
    if ((instruction & 0xF000) == 0x9000) {
        return INST_LOAD_STORE;
    }
    
    // Load address (1010x)
    if ((instruction & 0xF000) == 0xA000) {
        return INST_DATA_PROCESSING; // ADD to PC/SP
    }
    
    // Add offset to Stack Pointer and PUSH/POP (1011x)
    if ((instruction & 0xF000) == 0xB000) {
        if ((instruction & 0x0600) == 0x0400) {
            return INST_LOAD_STORE_MULTIPLE; // PUSH/POP
        }
        return INST_MISCELLANEOUS; // Add offset to SP
    }
    
    // Multiple load/store (1100x)
    if ((instruction & 0xF000) == 0xC000) {
        return INST_LOAD_STORE_MULTIPLE;
    }
    
    // Conditional branch and SWI (1101x)
    if ((instruction & 0xF000) == 0xD000) {
        if ((instruction & 0x0F00) == 0x0F00) {
            return INST_EXCEPTION; // SWI
        }
        return INST_BRANCH; // Conditional branch
    }
    
    // Unconditional branch (11100)
    if ((instruction & 0xF800) == 0xE000) {
        return INST_BRANCH;
    }
    
    // Long branch with link (1111x) - 32-bit instruction
    if ((instruction & 0xF000) == 0xF000) {
        return INST_BRANCH;
    }
    
    return INST_DATA_PROCESSING; // Default fallback
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

InstructionFields Instruction::decode_32bit_instruction(uint32_t instruction)
{
    InstructionFields fields = {};
    fields.opcode = instruction;
    fields.is_32bit = true;
    
    // Extract first and second halfwords
    uint16_t first_half = (instruction >> 16) & 0xFFFF;
    uint16_t second_half = instruction & 0xFFFF;
    
    // Basic 32-bit instruction decoding based on ARMv7-M patterns
    uint8_t op1 = (first_half >> 11) & 0x3;  // Bits [12:11]
    uint8_t op2 = (first_half >> 4) & 0x7F;  // Bits [10:4]
    uint8_t op = (second_half >> 15) & 0x1;  // Bit [15] of second halfword
    
    // Identify instruction class based on encoding table
    if (op1 == 0x01) {
        if ((op2 & 0x64) == 0x00) {
            // Load/store multiple
            fields.type = INST_LOAD_STORE_MULTIPLE;
            fields.rn = first_half & 0xF;
            fields.reg_list = second_half & 0xFFFF;
            fields.load_store_bit = (first_half & 0x10) != 0;
        } else if ((op2 & 0x64) == 0x04) {
            // Load/store dual or exclusive
            fields.type = INST_LOAD_STORE;
            // Basic decoding for dual/exclusive operations
            fields.rn = first_half & 0xF;
            fields.rd = (second_half >> 12) & 0xF;
            fields.rm = second_half & 0xF;
        } else {
            // Data processing (shifted register)
            fields.type = INST_DATA_PROCESSING;
            fields.rn = first_half & 0xF;
            fields.rd = (second_half >> 8) & 0xF;
            fields.rm = second_half & 0xF;
            fields.alu_op = (first_half >> 5) & 0xF;
        }
    } else if (op1 == 0x10) {
        if (op == 0) {
            // Data processing (modified immediate)
            fields.type = INST_DATA_PROCESSING;
            fields.rn = first_half & 0xF;
            fields.rd = (second_half >> 8) & 0xF;
            fields.alu_op = (first_half >> 5) & 0xF;
            // Extract modified immediate (complex encoding)
            fields.imm = second_half & 0xFF;
        } else {
            // Data processing (plain binary immediate)
            fields.type = INST_DATA_PROCESSING;
            fields.rd = (second_half >> 8) & 0xF;
            fields.imm = ((first_half & 0xF) << 12) | (second_half & 0xFF);
        }
    } else if (op1 == 0x11) {
        // Branches and miscellaneous control
        fields.type = INST_BRANCH;
        fields.cond = (first_half >> 6) & 0xF;
        // Extract branch offset (complex encoding)
        uint32_t imm11 = first_half & 0x7FF;
        uint32_t imm10 = second_half & 0x3FF;
        fields.imm = (imm11 << 12) | (imm10 << 1);
        
        // Sign extend 24-bit offset
        if (fields.imm & 0x800000) {
            fields.imm |= 0xFF000000;
        }
    } else {
        // Default classification
        fields.type = INST_DATA_PROCESSING;
        fields.rn = first_half & 0xF;
        fields.rd = (second_half >> 8) & 0xF;
        fields.rm = second_half & 0xF;
    }
    
    return fields;
}

bool Instruction::is_32bit_instruction(uint16_t first_half)
{
    // Check for 32-bit Thumb-2 instructions according to ARMv7-M specification
    // 32-bit instructions have bits [15:13] = 111 and bits [12:11] != 00
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