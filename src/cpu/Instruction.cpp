#include "Instruction.h"
#include "ARM_CortexM_Config.h"
#include "Log.h"
#include <sstream>

Instruction::Instruction(sc_module_name name) : sc_module(name)
{
    LOG_INFO("Instruction decoder initialized for " ARM_CORE_NAME " (" ARM_ARCH_NAME ")");
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
#if 0
    // Debug logging for decode verification
    if (Log::getInstance().get_log_level() >= LOG_DEBUG) {
        std::stringstream ss;
        if (fields.is_32bit) {
            // For 32-bit instructions, print as two 16-bit halfwords
            uint16_t first_half = (fields.opcode >> 16) & 0xFFFF;
            uint16_t second_half = fields.opcode & 0xFFFF;
            ss << std::hex << second_half << " " << std::hex << first_half;
        } else {
            // For 16-bit instructions, print as single halfword
            ss << std::hex << (fields.opcode & 0xFFFF);
        }
        if (fields.rd != 0xFF) ss << " rd=" << std::dec << (int)fields.rd;
        if (fields.rn != 0xFF) ss << " rn=" << std::dec << (int)fields.rn;
        if (fields.rm != 0xFF) ss << " rm=" << std::dec << (int)fields.rm;
        if (fields.rs != 0xFF) ss << " rs=" << std::dec << (int)fields.rs;
        if (fields.imm != 0) ss << " imm=0x" << std::hex << fields.imm;
        ss << " type=" << std::dec << fields.type;
        LOG_DEBUG("DECODE: opcode=0x" + ss.str());
    }
#endif
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
    
    // ... existing code ...
    
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
#if HAS_BLX_REGISTER
            case 3: fields.type = ((instruction & 0x0080) ? INST_T16_BLX : INST_T16_BX); break;
#else
            case 3: 
                if (instruction & 0x0080) {
                    // BLX not supported in this architecture, treat as undefined
                    LOG_WARNING("BLX instruction not supported in " ARM_CORE_NAME);
                    fields.type = INST_UNKNOWN;
                } else {
                    fields.type = INST_T16_BX;
                }
                break;
#endif
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

#if HAS_IT_BLOCKS  
    // IT instruction (ARMv7-M) - Format: 10111111 xxxxxxxx  
    // Must be checked before Format 12 to avoid conflict with ADD/SUB SP
    // IT{x{y{z}}} cond - If-Then block
    if ((instruction & 0xFF00) == 0xBF00) {
        uint32_t mask = instruction & 0xF;
        uint32_t firstcond = (instruction >> 4) & 0xF;
        
        // Check if this is actually an IT instruction (not other hint)
        if (mask != 0) {
            fields.type = INST_T16_IT;
            fields.cond = firstcond;
            fields.imm = mask; // IT mask
            return fields;
        }
        // Fall through to hint instruction handling for mask=0
        // (handled later in hint processing)
    }
#endif
    
    // Format 12: Add offset to Stack Pointer and PUSH/POP (1011xxxx)
    if ((opcode_field & 0x3C) == 0x2C) {
#if HAS_CBZ_CBNZ
        // First check for CBZ/CBNZ instructions (ARMv7-M) - Format: 1011xx1x xxxxxxxx
        // CBZ:  10110x01 xxxxxxxx (op=0)
        // CBNZ: 10111x01 xxxxxxxx (op=1)  
        if ((instruction & 0xF500) == 0xB100) {
            uint32_t op = (instruction >> 11) & 0x1;
            uint32_t i = (instruction >> 9) & 0x1;
            uint32_t rn = instruction & 0x7;
            uint32_t imm5 = (instruction >> 3) & 0x1F;
            
            fields.type = op ? INST_T16_CBNZ : INST_T16_CBZ;
            fields.rn = rn;
            // Compute branch offset: i:imm5:0 (6-bit offset, word-aligned)
            fields.imm = (i << 6) | (imm5 << 1);
            return fields;
        }
#endif
        
        // Check for CPS (Change Processor State): 1011 0110 011x xxxx (0xB660-0xB67F)
        if ((instruction & 0xFFE0) == 0xB660) {
            fields.type = INST_T16_CPS;
            fields.alu_op = (instruction & 0x10) ? 1 : 0; // 1=CPSID, 0=CPSIE  
            fields.imm = instruction & 0x7; // interrupt mask bits (I=1, F=2, IF=3)
            return fields;
        }
        
        // First check for extend/pack instructions: 1011001000xxxxxx (0xB200-0xB2FF)
        if ((instruction & 0xFF00) == 0xB200) {
            // Extend instructions: SXTH, SXTB, UXTH, UXTB
            fields.rd = instruction & 0x7;
            fields.rm = (instruction >> 3) & 0x7;
            uint32_t op = (instruction >> 6) & 0x3;
            switch (op) {
                case 0: // SXTH
                case 1: // SXTB  
                case 2: // UXTH
                case 3: // UXTB
                    fields.alu_op = op;
                    fields.type = INST_T16_EXTEND;
                    break;
            }
            return fields;
        }
        
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
        uint32_t hint_op = instruction & 0xFF;
        
#if HAS_EXTENDED_HINTS
        // ARMv7-M extended hints
        switch (hint_op) {
            case 0x10: fields.type = INST_T16_YIELD; break;
            case 0x20: fields.type = INST_T16_WFE; break;
            case 0x30: fields.type = INST_T16_WFI; break; 
            case 0x40: fields.type = INST_T16_SEV; break;
            default:   fields.type = INST_T16_HINT; break; // NOP and others
        }
#else
        // ARMv6-M basic hints (mainly NOP)
        fields.type = INST_T16_HINT;
#endif
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
    // Default T32 instructions are unconditional (AL)
    fields.cond = 0xE;
    
    // Extract first and second halfwords
    uint16_t first_half = instruction & 0xFFFF;
    uint16_t second_half = (instruction >> 16) & 0xFFFF;

#if HAS_T32_BL
    // T32 Branch Instructions (B.W and B<cond>.W)
    
    // T32 Unconditional branch (B.W) - 11110xxxxxxxxxxx 10x1xxxxxxxxxxxx
    // Pattern: F000 9xxx to F7FF 9xxx (but exclude F380-F3FF range for MSR/MRS/other system instructions)
    if ((first_half & 0xF800) == 0xF000 && (second_half & 0xD000) == 0x9000 && 
        !((first_half & 0xFF80) == 0xF380)) { // Exclude F380-F3FF range
        uint32_t S     = (first_half >> 10) & 0x1;
        uint32_t imm10 =  first_half        & 0x03FF; // [9:0]
        uint32_t J1    = (second_half >> 13) & 0x1;   // [13]
        uint32_t J2    = (second_half >> 11) & 0x1;   // [11]
        uint32_t imm11 =  second_half        & 0x07FF; // [10:0]

        uint32_t I1 = (~(J1 ^ S)) & 0x1;
        uint32_t I2 = (~(J2 ^ S)) & 0x1;

        // Assemble 25-bit immediate for unconditional branch
        uint32_t imm25 = (S << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1);
        int32_t signed_imm = (imm25 & 0x01000000) ? (int32_t)(imm25 | 0xFE000000) : (int32_t)imm25;

        fields.type = INST_T32_B;
        fields.cond = 0xE; // Always  
        fields.imm  = (uint32_t)signed_imm;
        return fields;
    }
    
    // T32 Conditional branch (B<cond>.W) - 11110xxxxxxxxxxx 10x0xxxxxxxxxxxx  
    // Pattern: F000 8xxx to F7FF 8xxx (but exclude F380-F3FF range for MSR/MRS/other system instructions)
    if ((first_half & 0xF800) == 0xF000 && (second_half & 0xD000) == 0x8000 &&
        !((first_half & 0xFF80) == 0xF380)) { // Exclude F380-F3FF range
        uint32_t S     = (first_half >> 10) & 0x1;
        uint32_t cond  = (first_half >> 6) & 0xF;  // Condition code
        uint32_t imm6  =  first_half       & 0x03F; // [5:0]
        uint32_t J1    = (second_half >> 13) & 0x1;   // [13]
        uint32_t J2    = (second_half >> 11) & 0x1;   // [11]
        uint32_t imm11 =  second_half       & 0x07FF; // [10:0]

        // For conditional branches, the immediate is smaller (20 bits)
        uint32_t imm20 = (S << 19) | (J2 << 18) | (J1 << 17) | (imm6 << 11) | imm11;
        // Apply left shift by 1 before sign extension
        uint32_t imm21 = imm20 << 1;
        int32_t signed_imm = (imm21 & 0x00100000) ? (int32_t)(imm21 | 0xFFE00000) : (int32_t)imm21;

        fields.type = INST_T32_B_COND;
        fields.cond = cond;
        fields.imm  = (uint32_t)signed_imm;
        return fields;
    }

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
#endif

#if HAS_MEMORY_BARRIERS
    // Memory barrier instructions (T32)
    // Format: 11110 01110111 1111 10 00 0000 option
    // DSB: f3bf 8f4f (option=15, SY)
    // DMB: f3bf 8f5f (option=15, SY) 
    // ISB: f3bf 8f6f (option=15, SY)
    if ((first_half & 0xFFF0) == 0xF3B0 && (second_half & 0xFF00) == 0x8F00) {
        uint32_t op = (second_half >> 4) & 0xF;
        uint32_t option = second_half & 0xF;

        switch (op) {
            case 0x4: // DSB
                fields.type = INST_T32_DSB;
                break;
            case 0x5: // DMB
                fields.type = INST_T32_DMB;
                break;
            case 0x6: // ISB
                fields.type = INST_T32_ISB;
                break;
            default:
                fields.type = INST_UNKNOWN;
                break;
        }

        fields.imm = option; // Store barrier option
        return fields;
    }
#endif

#if HAS_SYSTEM_REGISTERS
    // MSR instructions (T32)
    // Format: 11110 0111000 xxxx 1000 xxxx xxxx xxxx
    // f380 8809 - MSR PSP, r0
    // f380 8814 - MSR CONTROL, r0
    if ((first_half & 0xFFE0) == 0xF380 && (second_half & 0xFF00) == 0x8800) {
        fields.type = INST_T32_MSR;
        fields.rn = first_half & 0xF; // source register
        uint32_t spec_reg = second_half & 0xFF; // special register encoding
        fields.imm = spec_reg; // Store special register number
        return fields;
    }

    // MRS instructions (T32)
    // Format: 11110 0111110 1111 1000 xxxx xxxx xxxx
    // f3ef 8xxx - MRS rx, spec_reg
    if ((first_half & 0xFFFF) == 0xF3EF && (second_half & 0xF000) == 0x8000) {
        fields.type = INST_T32_MRS;
        fields.rd = (second_half >> 8) & 0xF; // destination register
        uint32_t spec_reg = second_half & 0xFF; // special register encoding
        fields.imm = spec_reg; // Store special register number
        return fields;
    }
#endif

#if SUPPORTS_ARMV7_M
    // --- T32 Load/Store Dual Instructions ---
    // LDRD/STRD format: 1110 100x x1x1 xxxx xxxx xxxx xxxx xxxx
    // LDRD: E8D0-E8DF, E950-E95F (immediate), E850-E85F (literal)
    // STRD: E8C0-E8CF, E940-E94F (immediate)
    if ((first_half & 0xFE50) == 0xE840 || (first_half & 0xFE50) == 0xE8C0 || 
        (first_half & 0xFE50) == 0xE850 || (first_half & 0xFE50) == 0xE8D0 ||
        (first_half & 0xFE50) == 0xE940 || (first_half & 0xFE50) == 0xE950) {
        
        bool is_load = ((first_half & 0x0020) != 0) || ((first_half & 0x0010) != 0);
        uint32_t rn = first_half & 0xF;
        uint32_t rt = (second_half >> 12) & 0xF;
        uint32_t rt2 = (second_half >> 8) & 0xF;
        uint32_t imm8 = second_half & 0xFF;
        
        // Convert imm8 to word offset (multiply by 4)
        uint32_t offset = imm8 << 2;
        
        fields.rd = rt;           // First register
        fields.rm = rt2;          // Second register
        fields.rn = rn;           // Base register
        fields.imm = offset;      // Offset (word-aligned)
        fields.load_store_bit = is_load;
        fields.type = is_load ? INST_T32_LDRD : INST_T32_STRD;
        return fields;
    }

    // --- T32 Data Processing (immediate) Instructions ---
    // These cover MOV.W, ADD.W, SUB.W, etc. with modified immediate constants
    // Format: 11110x0xxxxx xxxx 0xxx xxxx xxxx xxxx
    
    // Conditional T32 Data Processing - 1111 0xxx xxxx xxxx xxxx xxxx xxxx xxxx
    // These are conditional execution versions 
    // ADD.W: f5xx xxxx (where xx encodes operands and condition)
    if ((first_half & 0xFF00) == 0xF500) {
        // This is ADDNE.W r3, r2, #8192 pattern: f502 5300
        fields.type = INST_T32_ADD_IMM;
        fields.rn = first_half & 0xF;  // r2
        fields.rd = (second_half >> 8) & 0xF;  // r3
        fields.cond = 0x1; // NE condition
        
        // For this specific encoding, immediate is in different bit positions
        // f502 5300 -> immediate 0x2000 (8192)
        uint32_t imm12 = ((second_half & 0x0700) << 4) | (second_half & 0xFF);
        fields.imm = imm12;
        
        return fields;
    }
    
    // Special-case MOV.W immediate for power-of-two constants encoded as F44F xxxx
    // Example: f44f 5300 -> mov.w r3, #8192 (1 << (imm4+8)) when low imm8 == 0
    // Must be checked before general MOV/MVN immediate decoder
    if (first_half == 0xF44F) {
        uint32_t imm8 = second_half & 0xFF;
        uint32_t imm4 = (second_half >> 12) & 0xF;
        uint32_t rd = (second_half >> 8) & 0xF;
        if (imm8 == 0) {
            fields.type = INST_T32_MOV_IMM;
            fields.rd = rd;
            fields.rn = 0;
            uint32_t shift = imm4 + 8; // place single 1-bit into higher byte positions
            fields.imm = (shift < 32) ? (1u << shift) : 0u;
            return fields;
        }
    }
    
    // MOV immediate (32-bit) - 11110x0001001111 0xxx xxxx xxxx xxxx
    // f04f xxxx - MOV.W Rd, #imm 
    // f06f xxxx - MVN.W Rd, #imm
    if ((first_half & 0xFBF0) == 0xF040 && (first_half & 0x000F) == 0x000F) {
        bool is_mvn = (first_half & 0x0400) != 0;
        fields.type = is_mvn ? INST_T32_MVN_IMM : INST_T32_MOV_IMM;
        fields.rd = (second_half >> 8) & 0xF;
        
        // Decode modified immediate constant (T32 encoding)
        uint32_t i = (first_half >> 10) & 0x1;
        uint32_t imm3 = (second_half >> 12) & 0x7;
        uint32_t imm8 = second_half & 0xFF;
        fields.imm = decode_t32_modified_immediate(i, imm3, imm8);
        
        return fields;
    }
    
    // Data processing immediate instructions
    // Format: 11110x0xxxxx xxxx 0xxx xxxx xxxx xxxx
    if ((first_half & 0xFE00) == 0xF000 && (second_half & 0x8000) == 0x0000) {
        uint32_t op = (first_half >> 5) & 0xF;
        uint32_t rn = first_half & 0xF;
        uint32_t rd = (second_half >> 8) & 0xF;
        bool s_bit = (first_half & 0x0010) != 0;
        
        // Decode modified immediate constant
        uint32_t i = (first_half >> 10) & 0x1;
        uint32_t imm3 = (second_half >> 12) & 0x7;
        uint32_t imm8 = second_half & 0xFF;
        uint32_t imm12 = decode_t32_modified_immediate(i, imm3, imm8);
        
        fields.rn = rn;
        fields.rd = rd;
        fields.imm = imm12;
        fields.s_bit = s_bit;
        
        switch (op) {
            case 0x0: // AND
                fields.type = INST_T32_AND_IMM;
                break;
            case 0x1: // BIC
                fields.type = INST_T32_BIC_IMM;
                break;
            case 0x2: // ORR/MOV
                if (rn == 0xF) {
                    fields.type = INST_T32_MOV_IMM;
                    fields.rn = 0; // Clear rn for MOV
                } else {
                    fields.type = INST_T32_ORR_IMM;
                }
                break;
            case 0x3: // ORN/MVN
                if (rn == 0xF) {
                    fields.type = INST_T32_MVN_IMM;
                    fields.rn = 0; // Clear rn for MVN
                } else {
                    fields.type = INST_T32_ORR_IMM; // ORN treated as ORR for simplicity
                }
                break;
            case 0x4: // EOR
                fields.type = INST_T32_EOR_IMM;
                break;
            case 0x8: // ADD
                fields.type = INST_T32_ADD_IMM;
                break;
            case 0xA: // ADC
                fields.type = INST_T32_ADC_IMM;
                break;
            case 0xB: // SBC
                fields.type = INST_T32_SBC_IMM;
                break;
            case 0xD: // SUB
                fields.type = INST_T32_SUB_IMM;
                break;
            case 0xE: // RSB
                fields.type = INST_T32_RSB_IMM;
                break;
            default:
                fields.type = INST_UNKNOWN;
                break;
        }
        
        // Handle compare instructions (no destination register)
        if (s_bit && rd == 0xF) {
            switch (op) {
                case 0x0: fields.type = INST_T32_TST_IMM; break;
                case 0x4: fields.type = INST_T32_TEQ_IMM; break;
                case 0x8: fields.type = INST_T32_CMN_IMM; break;
                case 0xD: fields.type = INST_T32_CMP_IMM; break;
            }
        }
        
        return fields;
    }
    
    // --- T32 Data Processing (register) Instructions ---
    // Pattern: 11101010xxxx xxxx 0xxx xxxx xxxx xxxx (EA00-EAFF with bit 15 = 0)
    // Examples: ea17 0405 -> ands.w r4, r7, r5
    //           ea05 0506 -> and.w r5, r5, r6
    if ((first_half & 0xFF00) == 0xEA00 && (second_half & 0x8000) == 0x0000) {
        uint32_t op = (first_half >> 5) & 0xF; // Operation code bits 8:5
        bool s_bit = (first_half & 0x0010) != 0; // S bit for flags update
        uint32_t rn = first_half & 0xF; // First operand register
        uint32_t rd = (second_half >> 8) & 0xF; // Destination register
        uint32_t rm = second_half & 0xF; // Second operand register
    uint32_t shift_type = (second_half >> 4) & 0x3; // Shift type bits 5:4
    // imm5 is split as imm3:imm2 across halfword
    uint32_t imm3 = (second_half >> 12) & 0x7;  // bits 14:12
    uint32_t imm2 = (second_half >> 6) & 0x3;   // bits 7:6
    uint32_t shift_amount = (imm3 << 2) | imm2; // full 5-bit shift amount
        
        fields.rn = rn;
        fields.rd = rd;
        fields.rm = rm;
        fields.shift_type = shift_type;
        fields.shift_amount = shift_amount;
        fields.s_bit = s_bit;
        
        // Decode operation type
        switch (op) {
            case 0x0: // AND
                fields.type = s_bit ? INST_T32_ANDS_REG : INST_T32_AND_REG;
                break;
            case 0x1: // BIC
                fields.type = s_bit ? INST_T32_BICS_REG : INST_T32_BIC_REG;
                break;
            case 0x2: // ORR
                if (rn == 0xF) {
                    // MOV (register) alias: ORR with Rn = PC
                    fields.rn = 0; // not used
                    fields.type = s_bit ? INST_T32_MOVS_REG : INST_T32_MOV_REG;
                } else {
                    fields.type = s_bit ? INST_T32_ORRS_REG : INST_T32_ORR_REG;
                }
                break;
            case 0x3: // ORN (bitwise OR NOT)
                if (rn == 0xF) {
                    // MVN (register) alias: ORN with Rn = PC
                    fields.rn = 0; // not used
                    fields.type = s_bit ? INST_T32_MVNS_REG : INST_T32_MVN_REG;
                } else {
                    fields.type = s_bit ? INST_T32_ORNS_REG : INST_T32_ORN_REG;
                }
                break;
            case 0x4: // EOR
                fields.type = s_bit ? INST_T32_EORS_REG : INST_T32_EOR_REG;
                break;
            case 0x6: // PKH (Pack Halfword) - rarely used
                fields.type = INST_T32_PKH_REG;
                break;
            case 0x8: // ADD
                fields.type = s_bit ? INST_T32_ADDS_REG : INST_T32_ADD_REG;
                break;
            case 0xA: // ADC
                fields.type = s_bit ? INST_T32_ADCS_REG : INST_T32_ADC_REG;
                break;
            case 0xB: // SBC
                fields.type = s_bit ? INST_T32_SBCS_REG : INST_T32_SBC_REG;
                break;
            case 0xD: // SUB
                fields.type = s_bit ? INST_T32_SUBS_REG : INST_T32_SUB_REG;
                break;
            case 0xE: // RSB
                fields.type = s_bit ? INST_T32_RSBS_REG : INST_T32_RSB_REG;
                break;
            default:
                fields.type = INST_UNKNOWN;
                break;
        }
        
        // Handle compare instructions (no destination register)
        if (s_bit && rd == 0xF) {
            switch (op) {
                case 0x0: fields.type = INST_T32_TST_REG; break;
                case 0x4: fields.type = INST_T32_TEQ_REG; break;
                case 0x8: fields.type = INST_T32_CMN_REG; break;
                case 0xD: fields.type = INST_T32_CMP_REG; break;
            }
        }
        
        return fields;
    }
    
    // T32 Data Processing (register) - EBxx encoding (shift amount split as imm3:imm2)
    // Pattern: 1110 1011 op(4) rn(4) 0 imm3(3) rd(4) imm2(2) type(2) rm(4)
    // Example: eb03 1202 -> add.w r2, r3, r2, lsl #4
    if ((first_half & 0xFF00) == 0xEB00 && (second_half & 0x8000) == 0x0000) {
        uint32_t op = (first_half >> 5) & 0xF;      // Operation code bits 8:5
        bool s_bit = (first_half & 0x0010) != 0;    // S bit for flags update
        uint32_t rn = first_half & 0xF;             // First operand register
        uint32_t rd = (second_half >> 8) & 0xF;     // Destination register
        uint32_t rm = second_half & 0xF;            // Second operand register
        uint32_t shift_type = (second_half >> 4) & 0x3; // Shift type bits 5:4
        uint32_t imm3 = (second_half >> 12) & 0x7;  // imm3 bits 14:12
        uint32_t imm2 = (second_half >> 6) & 0x3;   // imm2 bits 7:6
        uint32_t shift_amount = (imm3 << 2) | imm2; // Full 5-bit shift amount

        fields.rn = rn;
        fields.rd = rd;
        fields.rm = rm;
        fields.shift_type = shift_type;
        fields.shift_amount = shift_amount;
        fields.s_bit = s_bit;

        switch (op) {
            case 0x0: fields.type = s_bit ? INST_T32_ANDS_REG : INST_T32_AND_REG; break;
            case 0x1: fields.type = s_bit ? INST_T32_BICS_REG : INST_T32_BIC_REG; break;
            case 0x2: fields.type = s_bit ? INST_T32_ORRS_REG : INST_T32_ORR_REG; break;
            case 0x3: fields.type = s_bit ? INST_T32_ORNS_REG : INST_T32_ORN_REG; break;
            case 0x4: fields.type = s_bit ? INST_T32_EORS_REG : INST_T32_EOR_REG; break;
            case 0x6: fields.type = INST_T32_PKH_REG; break;
            case 0x8: fields.type = s_bit ? INST_T32_ADDS_REG : INST_T32_ADD_REG; break;
            case 0xA: fields.type = s_bit ? INST_T32_ADCS_REG : INST_T32_ADC_REG; break;
            case 0xB: fields.type = s_bit ? INST_T32_SBCS_REG : INST_T32_SBC_REG; break;
            case 0xD: fields.type = s_bit ? INST_T32_SUBS_REG : INST_T32_SUB_REG; break;
            case 0xE: fields.type = s_bit ? INST_T32_RSBS_REG : INST_T32_RSB_REG; break;
            default:  fields.type = INST_UNKNOWN; break;
        }

        if (s_bit && rd == 0xF) {
            switch (op) {
                case 0x0: fields.type = INST_T32_TST_REG; break;
                case 0x4: fields.type = INST_T32_TEQ_REG; break;
                case 0x8: fields.type = INST_T32_CMN_REG; break;
                case 0xD: fields.type = INST_T32_CMP_REG; break;
            }
        }

        return fields;
    }
    
    // --- T32 Shift Instructions (register) ---
    // Pattern: 1111 1010 000S xxxx 1111 xxxx 00xx xxxx (FA0x Fxxx)
    // LSL.W/LSR.W/ASR.W/ROR.W Rd, Rm, Rs
    // Example: fa05 f302 -> lsl.w r3, r5, r2
    if ((first_half & 0xFFE0) == 0xFA00 && (second_half & 0xF0C0) == 0xF000) {
        bool s_bit = (first_half & 0x0010) != 0;    // S bit for flags update
        uint32_t rm = first_half & 0xF;             // Source register to shift
        uint32_t rd = (second_half >> 8) & 0xF;     // Destination register  
        uint32_t rs = second_half & 0xF;            // Register containing shift amount
        uint32_t shift_type = (second_half >> 4) & 0x3; // Shift type bits 5:4
        
        fields.rd = rd;
        fields.rm = rm; 
        fields.rn = rs; // Use rn field for shift amount register
        fields.shift_type = shift_type;
        fields.shift_amount = 0; // Not used for register shifts
        fields.s_bit = s_bit;
        
        // Decode shift operation type
        switch (shift_type) {
            case 0x0: // LSL
                fields.type = s_bit ? INST_T32_LSLS_REG : INST_T32_LSL_REG;
                break;
            case 0x1: // LSR  
                fields.type = s_bit ? INST_T32_LSRS_REG : INST_T32_LSR_REG;
                break;
            case 0x2: // ASR
                fields.type = s_bit ? INST_T32_ASRS_REG : INST_T32_ASR_REG;
                break;
            case 0x3: // ROR
                fields.type = s_bit ? INST_T32_RORS_REG : INST_T32_ROR_REG;
                break;
            default:
                fields.type = INST_UNKNOWN;
                break;
        }
        
        return fields;
    }
    
    // --- T32 Load/Store Instructions ---
    // LDR literal (PC-relative) - 11111000 x101 1111 xxxx xxxxxxxxxxxx
    // F8DF xxxx: LDR.W Rt, [PC, #±imm12]
    if ((first_half & 0xFF7F) == 0xF85F && (first_half & 0x000F) == 0x000F) {
        uint32_t rt = (second_half >> 12) & 0xF;
        uint32_t imm12 = second_half & 0xFFF;
        bool u_bit = (first_half & 0x0080) != 0; // Add/subtract bit
        
        fields.rn = 15; // PC
        fields.rd = rt;
        fields.imm = u_bit ? imm12 : (-imm12 & 0xFFFFFFFF);
        fields.load_store_bit = 1; // Load
        fields.type = INST_T32_LDR_PC; // LDR.W from PC
        return fields;
    }
    
    // LDR/STR immediate (positive offset)
    // STR.W Rt, [Rn, #imm12]: 1111 1000 1100 rrrr tttt iiii iiii iiii (F8C0-F8CF)
    // LDR.W Rt, [Rn, #imm12]: 1111 1000 1101 rrrr tttt iiii iiii iiii (F8D0-F8DF)
    if ((first_half & 0xFFE0) == 0xF8C0 || (first_half & 0xFFE0) == 0xF8D0) {
        bool is_load = (first_half & 0x0010) != 0; // D bit for load, C bit for store
        uint32_t rn = first_half & 0xF;
        uint32_t rt = (second_half >> 12) & 0xF;
        uint32_t imm12 = second_half & 0xFFF;
        
        // Skip PC-relative loads (handled above)
        if (rn != 15) {
            fields.rn = rn;
            fields.rd = rt;
            fields.imm = imm12;
            fields.load_store_bit = is_load;
            fields.type = is_load ? INST_T32_LDR_IMM : INST_T32_STR_IMM;
            return fields;
        }
    }

    // LDRB/STRB immediate (positive offset)
    // STRB.W Rt, [Rn, #imm12]: 1111 1000 1000 rrrr tttt iiii iiii iiii (F880-F88F)
    // LDRB.W Rt, [Rn, #imm12]: 1111 1000 1001 rrrr tttt iiii iiii iiii (F890-F89F)
    if ((first_half & 0xFFE0) == 0xF880 || (first_half & 0xFFE0) == 0xF890) {
        bool is_load = (first_half & 0x0010) != 0;
        uint32_t rn = first_half & 0xF;
        uint32_t rt = (second_half >> 12) & 0xF;
        uint32_t imm12 = second_half & 0xFFF;
        
        if (rn != 15) {
            fields.rn = rn;
            fields.rd = rt;
            fields.imm = imm12;
            fields.load_store_bit = is_load;
            fields.type = is_load ? INST_T32_LDRB_IMM : INST_T32_STRB_IMM;
            return fields;
        }
    }

    // LDRH/STRH immediate (positive offset)
    // STRH.W Rt, [Rn, #imm12]: 1111 1000 1010 rrrr tttt iiii iiii iiii (F8A0-F8AF)
    // LDRH.W Rt, [Rn, #imm12]: 1111 1000 1011 rrrr tttt iiii iiii iiii (F8B0-F8BF)
    if ((first_half & 0xFFE0) == 0xF8A0 || (first_half & 0xFFE0) == 0xF8B0) {
        bool is_load = (first_half & 0x0010) != 0;
        uint32_t rn = first_half & 0xF;
        uint32_t rt = (second_half >> 12) & 0xF;
        uint32_t imm12 = second_half & 0xFFF;
        
        if (rn != 15) {
            fields.rn = rn;
            fields.rd = rt;
            fields.imm = imm12;
            fields.load_store_bit = is_load;
            fields.type = is_load ? INST_T32_LDRH_IMM : INST_T32_STRH_IMM;
            return fields;
        }
    }

    // T32 Load/Store with immediate and pre/post-indexed addressing (F8xx pattern)
    // Pattern: 1111 1000 ULSW Rn Rt xxxx xxxx xxxx
    // U=1 for positive offset, L=1 for load, S=size (0=word,1=byte), W=1 for word vs halfword
    // F850-F85F: L=1 (load), S=0 (word) -> LDR.W
    // F840-F84F: L=0 (store), S=0 (word) -> STR.W  
    // F810-F81F: L=1 (load), S=1 (byte) -> LDRB.W
    // F800-F80F: L=0 (store), S=1 (byte) -> STRB.W
    if ((first_half & 0xFF00) == 0xF800 || (first_half & 0xFF00) == 0xF810 ||
        (first_half & 0xFF00) == 0xF820 || (first_half & 0xFF00) == 0xF830 ||
        (first_half & 0xFF00) == 0xF840 || (first_half & 0xFF00) == 0xF850) {

        // Extract L bit (bit 4 of first halfword) and determine byte vs word from pattern
        bool l_bit = (first_half & 0x0010) != 0; // Load=1, Store=0
        
        bool is_load = l_bit;
        bool is_byte = ((first_half & 0xFF00) == 0xF800) || ((first_half & 0xFF00) == 0xF810); // F800/F810 are byte operations
        uint32_t rn = first_half & 0xF;
        uint32_t rt = (second_half >> 12) & 0xF;

        bool has_index_bits = (second_half & 0x0800) != 0;   // bit11
        uint32_t imm8 = second_half & 0xFF;

        if (has_index_bits) {
            bool p_bit = (second_half & 0x0400) != 0;  // P
            bool u_bit = (second_half & 0x0200) != 0;  // U (add=1, sub=0)
            bool w_bit = (second_half & 0x0100) != 0;  // W

            // Pre-indexed: P=1,W=1
            if (p_bit && w_bit) {
                fields.rn = rn;
                fields.rd = rt;
                fields.imm = imm8; // Always store positive value
                fields.negative_offset = !u_bit; // U=0 means negative
                fields.load_store_bit = is_load;
                fields.pre_indexed = true;
                fields.writeback = true;
                if (is_byte) {
                    fields.type = is_load ? INST_T32_LDRB_PRE_POST : INST_T32_STRB_PRE_POST;
                } else {
                    fields.type = is_load ? INST_T32_LDR_PRE_POST : INST_T32_STR_PRE_POST;
                }
                return fields;
            }

            // Post-indexed: P=0,W=1
            if (!p_bit && w_bit) {
                fields.rn = rn;
                fields.rd = rt;
                fields.imm = imm8; // Always store positive value
                fields.negative_offset = !u_bit; // U=0 means negative
                fields.load_store_bit = is_load;
                fields.pre_indexed = false;
                fields.writeback = true;
                if (is_byte) {
                    fields.type = is_load ? INST_T32_LDRB_PRE_POST : INST_T32_STRB_PRE_POST;
                } else {
                    fields.type = is_load ? INST_T32_LDR_PRE_POST : INST_T32_STR_PRE_POST;
                }
                return fields;
            }

            // Offset addressing (no writeback): P=1,W=0 -> treat as immediate form
            if (p_bit && !w_bit) {
                fields.rn = rn;
                fields.rd = rt;
                fields.imm = imm8;
                fields.load_store_bit = is_load;
                fields.pre_indexed = false;
                fields.writeback = false;
                fields.negative_offset = !u_bit; // U=0 means subtract
                if (is_byte) {
                    fields.type = is_load ? INST_T32_LDRB_IMM : INST_T32_STRB_IMM;
                } else {
                    fields.type = is_load ? INST_T32_LDR_IMM : INST_T32_STR_IMM;
                }
                return fields;
            }

            // P=0,W=0 is undefined for this encoding; fall through to not match
        } else {
            // Legacy immediate negative-offset variant when bit11=0
            fields.rn = rn;
            fields.rd = rt;
            fields.imm = imm8; // negative offset indicated by pattern; executor/formatter uses negative_offset
            fields.load_store_bit = is_load;
            fields.pre_indexed = false;
            fields.writeback = false;
            fields.negative_offset = true;
            if (is_byte) {
                fields.type = is_load ? INST_T32_LDRB_IMM : INST_T32_STRB_IMM;
            } else {
                fields.type = is_load ? INST_T32_LDR_IMM : INST_T32_STR_IMM;
            }
            return fields;
        }
    }

    // LDRSB/LDRSH immediate (positive offset) - signed variants
    // LDRSB.W Rt, [Rn, #imm12]: 1111 1001 1001 rrrr tttt iiii iiii iiii (F990-F99F)
    // LDRSH.W Rt, [Rn, #imm12]: 1111 1001 1011 rrrr tttt iiii iiii iiii (F9B0-F9BF)
    if ((first_half & 0xFFE0) == 0xF990 || (first_half & 0xFFE0) == 0xF9B0) {
        bool is_halfword = (first_half & 0x0020) != 0; // Bit 5 distinguishes SB (0) vs SH (1)
        uint32_t rn = first_half & 0xF;
        uint32_t rt = (second_half >> 12) & 0xF;
        uint32_t imm12 = second_half & 0xFFF;
        
        if (rn != 15) {
            fields.rn = rn;
            fields.rd = rt;
            fields.imm = imm12;
            fields.load_store_bit = 1; // Always load
            fields.type = is_halfword ? INST_T32_LDRSH_IMM : INST_T32_LDRSB_IMM;
            return fields;
        }
    }
#endif

#if SUPPORTS_ARMV7_M
    // --- ARMv7-M Extended T32 Instructions ---
    
    // Multiple Load/Store Instructions (T32)
    // LDMIA: 11101000 1001 rrrr xxxx xxxx xxxx xxxx (E890-E89F)
    // LDMDB: 11101000 1011 rrrr xxxx xxxx xxxx xxxx (E8B0-E8BF) 
    // STMIA: 11101000 1000 rrrr xxxx xxxx xxxx xxxx (E880-E88F)
    // STMDB: 11101001 00x0 rrrr xxxx xxxx xxxx xxxx (E900-E92F)
    if (((first_half & 0xFF00) == 0xE800) || ((first_half & 0xFF00) == 0xE900)) {
        bool is_e9_series = (first_half & 0xFF00) == 0xE900;
        uint32_t rn = first_half & 0xF;
        uint16_t reg_list = second_half;
        
        fields.rn = rn;
        fields.reg_list = reg_list;
        
        if (is_e9_series) {
            // E900-E92F series: STMDB
            fields.load_store_bit = 0; // Store
            fields.type = INST_T32_STMDB;
        } else {
            // E800 series: determine based on bits [5:4]
            uint32_t op = (first_half >> 4) & 0x3;
            switch (op) {
                case 0: // STMIA (E880)
                    fields.load_store_bit = 0;
                    fields.type = INST_T32_STMIA;
                    break;
                case 1: // LDMIA (E890)
                    fields.load_store_bit = 1;
                    fields.type = INST_T32_LDMIA;
                    break;
                case 3: // LDMDB (E8B0)
                    fields.load_store_bit = 1;
                    fields.type = INST_T32_LDMDB;
                    break;
                default:
                    return fields; // Unknown encoding
            }
        }
        return fields;
    }    // Table Branch Instructions (TBB/TBH)
    // TBB [Rn, Rm]        - 11101000 1101 xxxx 1111 0000 0000 xxxx 
    // TBH [Rn, Rm, LSL#1] - 11101000 1101 xxxx 1111 0000 0001 xxxx
    if ((first_half & 0xFFF0) == 0xE8D0 && (second_half & 0xFFE0) == 0xF000) {
        bool is_halfword = (second_half & 0x0010) != 0;
        fields.type = is_halfword ? INST_T32_TBH : INST_T32_TBB;
        fields.rn = first_half & 0xF;           // Base register
        fields.rm = second_half & 0xF;          // Index register  
        return fields;
    }

    // Clear Exclusive (CLREX)
    // Format: 11110011 10111111 10000111 00100000 (F3BF 8720)
    if (first_half == 0xF3BF && second_half == 0x8720) {
        fields.type = INST_T32_CLREX;
        return fields;
    }

#if HAS_EXCLUSIVE_ACCESS
    // Exclusive Access Instructions
    // LDREX:  11101000 0101 xxxx xxxx 1111 xxxxxxxx (E850 xxFx)
    // STREX:  11101000 0100 xxxx xxxx 1111 xxxxxxxx (E840 xxFx)
    // LDREXB: 11101000 1101 xxxx xxxx 1111 0100xxxx (E8D0 xxF4)
    // STREXB: 11101000 1100 xxxx xxxx 1111 0100xxxx (E8C0 xxF4)
    // LDREXH: 11101000 1101 xxxx xxxx 1111 0101xxxx (E8D0 xxF5)
    // STREXH: 11101000 1100 xxxx xxxx 1111 0101xxxx (E8C0 xxF5)
    
    if ((first_half & 0xFFF0) == 0xE850 && (second_half & 0x00F0) == 0x00F0) {
        // LDREX Rt, [Rn, #imm8]
        fields.type = INST_T32_LDREX;
        fields.rn = first_half & 0xF;           // Base register
        fields.rd = (second_half >> 12) & 0xF;  // Destination register
        fields.imm = (second_half & 0xFF) << 2; // Offset (word-aligned)
        return fields;
    }
    
    if ((first_half & 0xFFF0) == 0xE840 && (second_half & 0x00F0) == 0x00F0) {
        // STREX Rd, Rt, [Rn, #imm8]
        fields.type = INST_T32_STREX;
        fields.rn = first_half & 0xF;           // Base register
        fields.rm = (second_half >> 12) & 0xF;  // Status register (Rd)
        fields.rd = (second_half >> 8) & 0xF;   // Source register (Rt)
        fields.imm = (second_half & 0xFF) << 2; // Offset (word-aligned)
        return fields;
    }
    
    if ((first_half & 0xFFF0) == 0xE8D0 && (second_half & 0x00FF) == 0x00F4) {
        // LDREXB Rt, [Rn]
        fields.type = INST_T32_LDREXB;
        fields.rn = first_half & 0xF;           // Base register
        fields.rd = (second_half >> 12) & 0xF;  // Destination register
        return fields;
    }
    
    if ((first_half & 0xFFF0) == 0xE8C0 && (second_half & 0x00FF) == 0x00F4) {
        // STREXB Rd, Rt, [Rn]
        fields.type = INST_T32_STREXB;
        fields.rn = first_half & 0xF;           // Base register
        fields.rm = (second_half >> 12) & 0xF;  // Status register (Rd)
        fields.rd = (second_half >> 8) & 0xF;   // Source register (Rt)
        return fields;
    }
    
    if ((first_half & 0xFFF0) == 0xE8D0 && (second_half & 0x00FF) == 0x00F5) {
        // LDREXH Rt, [Rn]
        fields.type = INST_T32_LDREXH;
        fields.rn = first_half & 0xF;           // Base register
        fields.rd = (second_half >> 12) & 0xF;  // Destination register
        return fields;
    }
    
    if ((first_half & 0xFFF0) == 0xE8C0 && (second_half & 0x00FF) == 0x00F5) {
        // STREXH Rd, Rt, [Rn]
        fields.type = INST_T32_STREXH;
        fields.rn = first_half & 0xF;           // Base register
        fields.rm = (second_half >> 12) & 0xF;  // Status register (Rd)
        fields.rd = (second_half >> 8) & 0xF;   // Source register (Rt)
        return fields;
    }
#endif

#if HAS_HARDWARE_DIVIDE
    // Hardware Divide Instructions
    // UDIV: 1111 1011 1011 rrrr 1111 dddd 1111 mmmm  (first=FBBx, second=F d F m)
    // SDIV: 1111 1011 1001 rrrr 1111 dddd 1111 mmmm  (first=FB9x, second=F d F m)
    if (((first_half & 0xFFF0) == 0xFBB0 || (first_half & 0xFFF0) == 0xFB90) &&
        (second_half & 0xF0F0) == 0xF0F0) {            // FIX: was 0xF000
        bool is_unsigned = (first_half & 0x0020) != 0;  // bit5 distinguishes UDIV(1)/SDIV(0)
        fields.type = is_unsigned ? INST_T32_UDIV : INST_T32_SDIV;
        fields.rn = first_half & 0xF;                  // dividend (Rn)
        fields.rd = (second_half >> 8) & 0xF;          // result   (Rd)
        fields.rm = second_half & 0xF;                 // divisor  (Rm)
        return fields;
    }
    
    // MLA/MLS: 1111 1011 0000 Rn Ra Rd op Rm (FB00-FB0F)
    // MLA: op = 0000, MLS: op = 0001
    if ((first_half & 0xFFF0) == 0xFB00) {
        uint32_t op = (second_half >> 4) & 0xF; // Extract op field
        
        if (op == 0x0) {
            fields.type = INST_T32_MLA;
        } else if (op == 0x1) {
            fields.type = INST_T32_MLS;
        } else {
            fields.type = INST_UNKNOWN; // Invalid op for MLA/MLS
        }
        
        fields.rn = first_half & 0xF;          // Multiplicand
        fields.rs = (second_half >> 12) & 0xF; // Addend (Ra)
        fields.rd = (second_half >> 8) & 0xF;  // Destination
        fields.rm = second_half & 0xF;         // Multiplier
        return fields;
    }
    
    // Long Multiply Instructions: 1111 1011 1010 Rn RdLo RdHi op Rm (FBA0-FBAF)
    // UMULL: op = 0000, SMULL: op = 0001, UMLAL: op = 0010, SMLAL: op = 0011
    if ((first_half & 0xFFF0) == 0xFBA0) {
        uint32_t op = (second_half >> 4) & 0xF; // Extract op field
        
        switch (op) {
            case 0x0:
                fields.type = INST_T32_UMULL;
                break;
            case 0x1:
                fields.type = INST_T32_SMULL;
                break;
            case 0x2:
                fields.type = INST_T32_UMLAL;
                break;
            case 0x3:
                fields.type = INST_T32_SMLAL_;
                break;
            default:
                fields.type = INST_UNKNOWN; // Invalid op for long multiply
                break;
        }
        
        fields.rn = first_half & 0xF;          // Rn (operand 1)
        fields.rd = (second_half >> 12) & 0xF; // RdLo (result low)
        fields.rs = (second_half >> 8) & 0xF;  // RdHi (result high)
        fields.rm = second_half & 0xF;         // Rm (operand 2)
        return fields;
    }
#endif

#if HAS_BIT_MANIPULATION
    // Bit Manipulation Instructions
    // CLZ:    11111010 1011 xxxx 1111 xxxx 1000 xxxx (FAB0 F08x)
    // RBIT:   11111010 1001 xxxx 1111 xxxx 1010 xxxx (FA90 F0Ax) 
    // REV:    11111010 1001 xxxx 1111 xxxx 1000 xxxx (FA90 F08x)
    // REV16:  11111010 1001 xxxx 1111 xxxx 1001 xxxx (FA90 F09x)
    // REVSH:  11111010 1001 xxxx 1111 xxxx 1011 xxxx (FA90 F0Bx)
    
    // CLZ - Count Leading Zeros
    if ((first_half & 0xFFF0) == 0xFAB0 && (second_half & 0xFFF0) == 0xF080) {
        fields.type = INST_T32_CLZ;
        fields.rm = first_half & 0xF;           // Source register
        fields.rd = (second_half >> 8) & 0xF;   // Destination register
        return fields;
    }
    
    // RBIT - Reverse Bits
    if ((first_half & 0xFFF0) == 0xFA90 && (second_half & 0xFFF0) == 0xF0A0) {
        fields.type = INST_T32_RBIT;
        fields.rm = first_half & 0xF;           // Source register
        fields.rd = (second_half >> 8) & 0xF;   // Destination register
        return fields;
    }
    
    // REV - Byte-Reverse Word
    if ((first_half & 0xFFF0) == 0xFA90 && (second_half & 0xFFF0) == 0xF080) {
        fields.type = INST_T32_REV;
        fields.rm = first_half & 0xF;           // Source register
        fields.rd = (second_half >> 8) & 0xF;   // Destination register
        return fields;
    }
    
    // REV16 - Byte-Reverse Packed Halfword  
    if ((first_half & 0xFFF0) == 0xFA90 && (second_half & 0xFFF0) == 0xF090) {
        fields.type = INST_T32_REV16;
        fields.rm = first_half & 0xF;           // Source register
        fields.rd = (second_half >> 8) & 0xF;   // Destination register
        return fields;
    }
    
    // REVSH - Byte-Reverse Signed Halfword
    if ((first_half & 0xFFF0) == 0xFA90 && (second_half & 0xFFF0) == 0xF0B0) {
        fields.type = INST_T32_REVSH;
        fields.rm = first_half & 0xF;           // Source register
        fields.rd = (second_half >> 8) & 0xF;   // Destination register
        return fields;
    }
#endif

#if HAS_BITFIELD_INSTRUCTIONS
    // Bit Field Instructions
    // BFI:  11110011 0110 xxxx 0xxx xxxx xxxx xxxx (F360)
    // BFC:  11110011 0110 1111 0xxx xxxx xxxx xxxx (F36F)
    // UBFX: 11111000 1100 xxxx 0xxx xxxx xxxx xxxx (F8C0)
    // SBFX: 11110011 0100 xxxx 0xxx xxxx xxxx xxxx (F340)
    
    // BFI - Bit Field Insert
    if ((first_half & 0xFFF0) == 0xF360 && (first_half & 0xF) != 0xF) {
        fields.type = INST_T32_BFI;
        fields.rn = first_half & 0xF;           // Source
        fields.rd = (second_half >> 8) & 0xF;   // Destination
        fields.imm = ((second_half & 0x7000) >> 10) | ((second_half & 0xC0) >> 6); // msb:lsb
        return fields;
    }
    
    // BFC - Bit Field Clear
    if (first_half == 0xF36F) {
        fields.type = INST_T32_BFC;
        fields.rd = (second_half >> 8) & 0xF;   // Destination
        fields.imm = ((second_half & 0x7000) >> 10) | ((second_half & 0xC0) >> 6); // msb:lsb
        return fields;
    }
    
    // UBFX - Unsigned Bit Field Extract
    if ((first_half & 0xFFC0) == 0xF3C0) {
        fields.type = INST_T32_UBFX;
        fields.rn = first_half & 0xF;           // Source
        fields.rd = (second_half >> 8) & 0xF;   // Destination  
        fields.imm = ((second_half & 0x7000) >> 10) | ((second_half & 0xC0) >> 6); // msb:lsb
        return fields;
    }
    
    // SBFX - Signed Bit Field Extract  
    if ((first_half & 0xFFC0) == 0xF340) {
        fields.type = INST_T32_SBFX;
        fields.rn = first_half & 0xF;           // Source
        fields.rd = (second_half >> 8) & 0xF;   // Destination
        fields.imm = ((second_half & 0x7000) >> 10) | ((second_half & 0xC0) >> 6); // msb:lsb  
        return fields;
    }
#endif

#if HAS_SATURATING_ARITHMETIC
    // Saturating Arithmetic Instructions
    // SSAT: 11110011 00xx xxxx 0xxx xxxx xxxx xxxx (F300-F33F)
    // USAT: 11110011 10xx xxxx 0xxx xxxx xxxx xxxx (F380-F3BF)
    
    // SSAT - Signed Saturate
    if ((first_half & 0xFFC0) == 0xF300) {
        fields.type = INST_T32_SSAT;
        fields.rn = first_half & 0xF;           // Source register
        fields.rd = (second_half >> 8) & 0xF;   // Destination
        fields.imm = ((first_half & 0x1F) << 16) | (second_half & 0x7F); // sat_imm:shift
        return fields;
    }
    
    // USAT - Unsigned Saturate
    if ((first_half & 0xFFC0) == 0xF380) {
        fields.type = INST_T32_USAT;
        fields.rn = first_half & 0xF;           // Source register  
        fields.rd = (second_half >> 8) & 0xF;   // Destination
        fields.imm = ((first_half & 0x1F) << 16) | (second_half & 0x7F); // sat_imm:shift
        return fields;
    }
#endif
#endif // SUPPORTS_ARMV7_M

    // Unknown/unsupported T32 instruction for this architecture
    fields.type = INST_UNKNOWN;
    return fields;
}

bool Instruction::is_32bit_instruction(uint32_t instruction)
{
    // Check if this is a 32-bit Thumb-2 instruction
    uint16_t first_half = (uint16_t)(instruction & 0xFFFF);
    
    // 32-bit instructions start with specific bit patterns in first halfword:
    // 11101 - Load/Store multiple, Load/Store dual, Table branch, etc.
    // 11110 - BL and other T32 instructions  
    // 11111 - More T32 instructions
    return ((first_half & 0xF800) == 0xE800) || 
           ((first_half & 0xF800) == 0xF000) || 
           ((first_half & 0xF800) == 0xF800);
}

// ARM ARM shared functions
static inline uint32_t ROR_C(uint32_t x, uint32_t shift, bool& carry_out)
{
    if (shift == 0) {
        carry_out = false; // carry_out unchanged
        return x;
    }
    shift = shift % 32;
    if (shift == 0) {
        carry_out = (x & 0x80000000) != 0;
        return x;
    }
    carry_out = (x & (1u << (shift - 1))) != 0;
    return (x >> shift) | (x << (32 - shift));
}

static inline uint32_t ROR(uint32_t x, uint32_t shift)
{
    bool dummy_carry;
    return ROR_C(x, shift, dummy_carry);
}

// ThumbExpandImm_C from ARM ARM A7.3.3
static inline uint32_t ThumbExpandImm_C(uint32_t imm12, bool carry_in, bool& carry_out)
{
    if ((imm12 >> 10) == 0) {
        // imm12<11:10> == '00'
        carry_out = carry_in;
        switch ((imm12 >> 8) & 0x3) {
            case 0:
                // imm12<9:8> == '00': imm32 = ZeroExtend(imm12<7:0>, 32)
                return imm12 & 0xFF;
            case 1:
                // imm12<9:8> == '01': imm32 = '00000000' : imm12<7:0> : '00000000' : imm12<7:0>
                if ((imm12 & 0xFF) == 0) {
                    // UNPREDICTABLE
                    return 0;
                }
                return ((imm12 & 0xFF) << 16) | (imm12 & 0xFF);
            case 2:
                // imm12<9:8> == '10': imm32 = imm12<7:0> : '00000000' : imm12<7:0> : '00000000'
                if ((imm12 & 0xFF) == 0) {
                    // UNPREDICTABLE
                    return 0;
                }
                return ((imm12 & 0xFF) << 24) | ((imm12 & 0xFF) << 8);
            case 3:
                // imm12<9:8> == '11': imm32 = imm12<7:0> : imm12<7:0> : imm12<7:0> : imm12<7:0>
                if ((imm12 & 0xFF) == 0) {
                    // UNPREDICTABLE
                    return 0;
                }
                return ((imm12 & 0xFF) << 24) | ((imm12 & 0xFF) << 16) | 
                       ((imm12 & 0xFF) << 8) | (imm12 & 0xFF);
        }
    } else {
        // imm12<11:10> != '00'
        // unrotated_value = ZeroExtend('1':imm12<6:0>, 32)
        uint32_t unrotated_value = 0x80 | (imm12 & 0x7F);
        // (imm32, carry_out) = ROR_C(unrotated_value, UInt(imm12<11:7>), carry_in)
        return ROR_C(unrotated_value, (imm12 >> 7) & 0x1F, carry_out);
    }
    return 0; // Should never reach here
}

static inline uint32_t ThumbExpandImm(uint32_t imm12)
{
    bool dummy_carry = false;
    return ThumbExpandImm_C(imm12, false, dummy_carry);
}

uint32_t Instruction::decode_t32_modified_immediate(uint32_t i, uint32_t imm3, uint32_t imm8)
{
    // Build imm12 = i:imm3:imm8
    uint32_t imm12 = (i << 11) | (imm3 << 8) | imm8;
    
    // Use ARM ARM standard ThumbExpandImm function
    return ThumbExpandImm(imm12);
}
