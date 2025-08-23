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
    fields.type = INST_UNKNOWN;
    
    if (is_32bit) {
        // Handle 32-bit Thumb instructions
        fields = decode_thumb32_instruction(instruction);
    } else {
        // Handle 16-bit Thumb instructions
        fields = decode_thumb16_instruction(instruction & 0xFFFF);
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

InstructionFields Instruction::decode_thumb16_instruction(uint16_t instruction)
{
    InstructionFields fields = {};
    fields.opcode = instruction;
    fields.is_32bit = false;
    // Initialize invalid register markers and sensible defaults
    fields.rd = 0xFF;
    fields.rn = 0xFF;
    fields.rm = 0xFF;
    fields.rs = 0xFF;
    fields.s_bit = false;
    
    // Extract opcode field (bits 15:10) for format identification
    uint16_t opcode_field = (instruction >> 10) & 0x3F;
    
    // Format 1: Move shifted register (000000-000101, excluding 000110-000111)
    if ((opcode_field & 0x38) == 0x00 && (opcode_field & 0x06) != 0x06) {
        fields.rd = instruction & 0x7;
        fields.rm = (instruction >> 3) & 0x7;
        fields.shift_amount = (instruction >> 6) & 0x1F;
        fields.shift_type = (instruction >> 11) & 0x3;
        switch (fields.shift_type) {
            case 0: fields.type = INST_T16_LSL_IMM; break;
            case 1: fields.type = INST_T16_LSR_IMM; break;
            case 2: fields.type = INST_T16_ASR_IMM; break;
            default: fields.type = INST_UNKNOWN; break;
        }
        return fields;
    }
    
    // Format 2: Add/subtract (000110, 000111)
    if ((opcode_field & 0x3E) == 0x06) {
        fields.rd = instruction & 0x7;
        fields.rn = (instruction >> 3) & 0x7;
        bool immediate_flag = (instruction & 0x0400) != 0;
        if (immediate_flag) {
            fields.imm = (instruction >> 6) & 0x7;
            fields.rm = 0xFF; // Mark as immediate
        } else {
            fields.rm = (instruction >> 6) & 0x7;
        }
        fields.alu_op = (instruction & 0x0200) ? 2 : 1; // 0=ADD, 1=SUB
        // Thumb ADD/SUB (Format 2) set flags (ADDS/SUBS)
        fields.s_bit = true;
        if (immediate_flag) {
            fields.type = (fields.alu_op == 1) ? INST_T16_ADD_IMM3 : INST_T16_SUB_IMM3;
        } else {
            fields.type = (fields.alu_op == 1) ? INST_T16_ADD_REG : INST_T16_SUB_REG;
        }
        return fields;
    }
    
    // Format 3: Move/compare/add/subtract immediate (00100x,00101x,00110x,00111x)
    if ((instruction & 0xE000) == 0x2000) {
        fields.rd = (instruction >> 8) & 0x7;
        fields.rn = fields.rd;
        fields.imm = instruction & 0xFF;
        fields.alu_op = (instruction >> 11) & 0x3;
        fields.rm = 0xFF; // immediate operand marker
        // MOVS/ADDS/SUBS/CMP (immediate) set flags
        fields.s_bit = true;
        switch (fields.alu_op) {
            case 0: fields.type = INST_T16_MOV_IMM; break;
            case 1: fields.type = INST_T16_CMP_IMM; break;
            case 2: fields.type = INST_T16_ADD_IMM8; break;
            case 3: fields.type = INST_T16_SUB_IMM8; break;
        }
        return fields;
    }
    
    // Format 4: ALU operations (010000)
    if (opcode_field == 0x10) {
        fields.rd = instruction & 0x7;
        fields.rn = fields.rd;
        fields.rm = (instruction >> 3) & 0x7;
        fields.alu_op = (instruction >> 6) & 0xF;
        static const InstructionType table[16] = {
            /*0*/ INST_T16_AND,
            /*1*/ INST_T16_EOR,
            /*2*/ INST_T16_LSL_REG,
            /*3*/ INST_T16_LSR_REG,
            /*4*/ INST_T16_ASR_REG,
            /*5*/ INST_T16_ADC,
            /*6*/ INST_T16_SBC,
            /*7*/ INST_T16_ROR,
            /*8*/ INST_T16_TST,
            /*9*/ INST_T16_NEG,
            /*10*/ INST_T16_CMP_REG,
            /*11*/ INST_T16_CMN,
            /*12*/ INST_T16_ORR,
            /*13*/ INST_T16_MUL,
            /*14*/ INST_T16_BIC,
            /*15*/ INST_T16_MVN
        };
        fields.type = table[fields.alu_op & 0xF];
        return fields;
    }
    
    // Format 5: Hi register operations/branch exchange (010001)
    if (opcode_field == 0x11) {
        fields.rd = instruction & 0x7;
        fields.rm = (instruction >> 3) & 0x7;
        fields.h1 = (instruction & 0x80) != 0;
        fields.h2 = (instruction & 0x40) != 0;
        fields.alu_op = (instruction >> 8) & 0x3;
        if (fields.h1) fields.rd += 8;
        if (fields.h2) fields.rm += 8;
        fields.cond = 0xE; // Unconditional
        switch (fields.alu_op) {
            case 0: fields.type = INST_T16_ADD_HI; break;
            case 1: fields.type = INST_T16_CMP_HI; break;
            case 2: fields.type = INST_T16_MOV_HI; break;
            case 3: fields.type = ((instruction & 0x0080) ? INST_T16_BLX : INST_T16_BX); break;
        }
        return fields;
    }
    
    // Format 6: PC-relative load (01001x)
    if ((opcode_field & 0x3E) == 0x12) {
        fields.rd = (instruction >> 8) & 0x7;
        fields.imm = (instruction & 0xFF) * 4;
        fields.rn = 15; // PC
        fields.load_store_bit = true;
        fields.byte_word = false;
        fields.type = INST_T16_LDR_PC;
        return fields;
    }
    
    // Format 7: Load/store with register offset + sign-extended (0101xx)
    if ((opcode_field & 0x3C) == 0x14) {
        fields.rd = instruction & 0x7;
        fields.rn = (instruction >> 3) & 0x7;
        fields.rm = (instruction >> 6) & 0x7;
        
        uint8_t op3 = (instruction >> 9) & 0x7;
        switch (op3) {
            case 0b000: // STR
                fields.load_store_bit = false; fields.byte_word = 0; break;
            case 0b001: // STRH
                fields.load_store_bit = false; fields.byte_word = 2; fields.alu_op = 0; break;
            case 0b010: // STRB
                fields.load_store_bit = false; fields.byte_word = 1; break;
            case 0b011: // LDRSB (sign-extended)
                fields.load_store_bit = true; fields.byte_word = 1; fields.alu_op = 1; break;
            case 0b100: // LDR
                fields.load_store_bit = true; fields.byte_word = 0; break;
            case 0b101: // LDRH
                fields.load_store_bit = true; fields.byte_word = 2; fields.alu_op = 2; break;
            case 0b110: // LDRB
                fields.load_store_bit = true; fields.byte_word = 1; break;
            case 0b111: // LDRSH (sign-extended)
                fields.load_store_bit = true; fields.byte_word = 2; fields.alu_op = 3; break;
        }
        static const InstructionType map_t16_ls_reg[8] = {
            INST_T16_STR_REG,
            INST_T16_STRH_REG,
            INST_T16_STRB_REG,
            INST_T16_LDRSB_REG,
            INST_T16_LDR_REG,
            INST_T16_LDRH_REG,
            INST_T16_LDRB_REG,
            INST_T16_LDRSH_REG,
        };
        fields.type = map_t16_ls_reg[op3];
        return fields;
    }
    
    // Format 8: Load/store with immediate offset (011xxx)
    if ((opcode_field & 0x38) == 0x18) {
        fields.rd = instruction & 0x7;
        fields.rn = (instruction >> 3) & 0x7;
        fields.imm = (instruction >> 6) & 0x1F;
        fields.load_store_bit = (instruction & 0x0800) != 0;
        fields.byte_word = (instruction & 0x1000) != 0;
        if (!fields.byte_word) fields.imm *= 4; // Scale for word access
        if (!fields.byte_word && !fields.load_store_bit) fields.type = INST_T16_STR_IMM;
        else if (!fields.byte_word && fields.load_store_bit) fields.type = INST_T16_LDR_IMM;
        else if (fields.byte_word && !fields.load_store_bit) fields.type = INST_T16_STRB_IMM;
        else fields.type = INST_T16_LDRB_IMM;
        return fields;
    }
    
    // Format 9: Load/store halfword (1000xx)
    if ((opcode_field & 0x3C) == 0x20) {
        fields.rd = instruction & 0x7;
        fields.rn = (instruction >> 3) & 0x7;
        fields.imm = ((instruction >> 6) & 0x1F) * 2;
        fields.load_store_bit = (instruction & 0x0800) != 0;
        fields.byte_word = 2; // Halfword
        fields.type = fields.load_store_bit ? INST_T16_LDRH_IMM : INST_T16_STRH_IMM;
        return fields;
    }
    
    // Format 10: SP-relative load/store (1001xx)
    if ((opcode_field & 0x3C) == 0x24) {
        fields.rd = (instruction >> 8) & 0x7;
        fields.imm = (instruction & 0xFF) * 4;
        fields.rn = 13; // SP
        fields.load_store_bit = (instruction & 0x0800) != 0;
        fields.byte_word = false;
        fields.type = fields.load_store_bit ? INST_T16_LDR_SP : INST_T16_STR_SP;
        return fields;
    }
    
    // Format 11: Load address (1010xx)
    if ((opcode_field & 0x3C) == 0x28) {
        fields.rd = (instruction >> 8) & 0x7;
        fields.imm = (instruction & 0xFF) * 4;
        fields.rn = (instruction & 0x0800) ? 13 : 15; // SP or PC
        fields.alu_op = 1; // ADD
        fields.type = (fields.rn == 15) ? INST_T16_ADD_PC : INST_T16_ADD_SP;
        return fields;
    }
    
    // Format 12: Add offset to Stack Pointer and PUSH/POP (1011xxxx)
    if ((opcode_field & 0x3C) == 0x2C) {
        // Check for PUSH/POP: bits 11:9 should be x1xx (bit 9 = 1)
        if ((instruction & 0x0600) == 0x0400) {
            fields.reg_list = instruction & 0xFF;
            fields.load_store_bit = (instruction & 0x0800) != 0;
            fields.rn = 13; // SP
            bool r_bit = (instruction & 0x0100) != 0;
            if (r_bit) {
                if (fields.load_store_bit) {
                    fields.reg_list |= 0x8000; // Include PC in POP
                } else {
                    fields.reg_list |= 0x4000; // Include LR in PUSH
                }
            }
            fields.type = fields.load_store_bit ? INST_T16_POP : INST_T16_PUSH;
        } else {
            fields.imm = (instruction & 0x7F) * 4;
            fields.alu_op = (instruction & 0x80) ? 2 : 1; // SUB or ADD
            fields.rd = fields.rn = 13; // SP
            fields.type = (instruction & 0x80) ? INST_T16_SUB_SP_IMM7 : INST_T16_ADD_SP_IMM7;
        }
        return fields;
    }
    
    // Format 13: Multiple load/store (1100xx)
    if ((opcode_field & 0x3C) == 0x30) {
        fields.rn = (instruction >> 8) & 0x7;
        fields.reg_list = instruction & 0xFF;
        fields.load_store_bit = (instruction & 0x0800) != 0;
        fields.type = fields.load_store_bit ? INST_T16_LDMIA : INST_T16_STMIA;
        return fields;
    }
    
    // Format 14: Conditional branch (1101xxxx, but not 11011111)
    if ((opcode_field & 0x3C) == 0x34) {
        // Software interrupt (11011111)
        if ((instruction & 0xFF00) == 0xDF00) {
            fields.type = INST_T16_SVC;
            fields.imm = instruction & 0xFF;
            return fields;
        }
        // Conditional branch
        fields.type = INST_T16_B_COND;
        fields.cond = (instruction >> 8) & 0xF;
        // Compute 9-bit signed offset: imm8:1 sign-extended
        uint32_t imm8 = instruction & 0xFF;
        uint32_t off1 = (imm8 << 1);         // 9-bit value
        int32_t soff = (int32_t)(off1 << 23) >> 23; // sign-extend 9-bit to 32-bit
        fields.imm = (uint32_t)soff;
        return fields;
    }
    
    // Format 15: Unconditional branch (11100xxx)
    if ((opcode_field & 0x38) == 0x38 && (opcode_field & 0x04) == 0x00) {
        fields.type = INST_T16_B;
        fields.cond = 0xE; // Always
        // Compute 12-bit signed offset: imm11:1 sign-extended
        uint32_t imm11 = instruction & 0x7FF;
        uint32_t off1 = (imm11 << 1);        // 12-bit value
        int32_t soff = (int32_t)(off1 << 20) >> 20; // sign-extend 12-bit to 32-bit
        fields.imm = (uint32_t)soff;
        return fields;
    }
    
    // Format 16: Long branch with link first half (1111xxxx)
    if ((opcode_field & 0x3C) == 0x3C) {
        fields.type = INST_T32_BL;
        fields.cond = 0xE;
        fields.alu_op = 1; // BL marker
        fields.imm = 0; // Will be fully decoded in decode_thumb32_instruction
        return fields;
    }
    
    // BKPT instruction (10111110xxxxxxxx)
    if ((instruction & 0xFF00) == 0xBE00) {
        fields.type = INST_T16_BKPT;
        return fields;
    }
    
    // HINT instructions (10111111xxxxxxxx) - NOP, WFI, WFE, etc.
    if ((instruction & 0xFF00) == 0xBF00) {
        fields.type = INST_T16_HINT;
        return fields;
    }
    
    // Unknown instruction
    fields.type = INST_UNKNOWN;
    return fields;
}

InstructionFields Instruction::decode_thumb32_instruction(uint32_t instruction)
{
    InstructionFields fields = {};
    fields.opcode = instruction;
    fields.is_32bit = true;
    
    // Extract first and second halfwords
    uint16_t first_half = instruction & 0xFFFF;
    uint16_t second_half = (instruction >> 16) & 0xFFFF;

    // ARMv6-M supports only BL as 32-bit T32 instruction
    if ((first_half & 0xF800) == 0xF000 && (second_half & 0xD000) == 0xD000) {
        uint32_t S     = (first_half >> 10) & 0x1;
        uint32_t imm10 =  first_half        & 0x03FF; // [9:0]
        uint32_t J1    = (second_half >> 13) & 0x1;   // [13]
        uint32_t J2    = (second_half >> 11) & 0x1;   // [11]
        uint32_t imm11 =  second_half        & 0x07FF; // [10:0]

        uint32_t I1 = (~(J1 ^ S)) & 0x1;
        uint32_t I2 = (~(J2 ^ S)) & 0x1;

        // Assemble 25-bit immediate with implicit low bit = 0 (Thumb)
        uint32_t imm25 = (S << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1);
        int32_t signed_imm = (imm25 & 0x01000000) ? (int32_t)(imm25 | 0xFE000000) : (int32_t)imm25;

    fields.type = INST_T32_BL;
        fields.cond = 0xE; // Always
        // For compatibility with Execute::execute_branch (which multiplies imm by 2),
        // store halfword offset here
        fields.imm  = (uint32_t)(signed_imm / 2);
        fields.alu_op = 1; // BL marker
        return fields;
    }

    // Unknown/unsupported T32 on ARMv6-M
    fields.type = INST_UNKNOWN;
    return fields;
}

bool Instruction::is_32bit_instruction(uint32_t instruction)
{
    // ARMv6-M: Only BL is 32-bit (first halfword 11110)
    uint16_t first_half = (uint16_t)(instruction & 0xFFFF);
    return (first_half & 0xF800) == 0xF000;
}
