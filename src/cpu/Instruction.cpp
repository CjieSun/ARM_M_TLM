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
    // Initialize all fields to zero/defaults
    InstructionFields fields = {};

    if (is_32bit) {
        // Handle 32-bit Thumb instructions
        fields = decode_thumb32_instruction(((instruction & 0xFFFF) << 16) | ((instruction >> 16) & 0xFFFF));
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
    
    // Format 1: Move shifted register (000xxxxx xxxxxxxx, excluding 000110xx-000111xx)
    if ((instruction & 0xE000) == 0x0000 && (instruction & 0x1800) != 0x1800) {
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

    // Format 2: Add/subtract (000110xx xxxxxxxx, 000111xx xxxxxxxx)
    if ((instruction & 0xF800) == 0x1800) {
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

    // Format 4: ALU operations (010000xx xxxxxxxx)
    if ((instruction & 0xFC00) == 0x4000) {
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

    // Format 5: Hi register operations/branch exchange (010001xx xxxxxxxx)
    if ((instruction & 0xFC00) == 0x4400) {
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

    // Format 6: PC-relative load (01001xxx xxxxxxxx)
    if ((instruction & 0xF800) == 0x4800) {
        fields.rd = (instruction >> 8) & 0x7;
        fields.imm = (instruction & 0xFF) * 4;
        fields.rn = 15; // PC
        fields.load_store_bit = true;
        fields.byte_word = false;
        fields.type = INST_T16_LDR_PC;
        return fields;
    }
    
    // Format 7: Load/store with register offset + sign-extended (0101xxxx xxxxxxxx)
    if ((instruction & 0xF000) == 0x5000) {
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

    // Format 8: Load/store with immediate offset (011xxxxx xxxxxxxx)
    if ((instruction & 0xE000) == 0x6000) {
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

    // Format 9: Load/store halfword (1000xxxx xxxxxxxx)
    if ((instruction & 0xF000) == 0x8000) {
        fields.rd = instruction & 0x7;
        fields.rn = (instruction >> 3) & 0x7;
        fields.imm = ((instruction >> 6) & 0x1F) * 2;
        fields.load_store_bit = (instruction & 0x0800) != 0;
        fields.byte_word = 2; // Halfword
        fields.type = fields.load_store_bit ? INST_T16_LDRH_IMM : INST_T16_STRH_IMM;
        return fields;
    }

    // Format 10: SP-relative load/store (1001xxxx xxxxxxxx)
    if ((instruction & 0xF000) == 0x9000) {
        fields.rd = (instruction >> 8) & 0x7;
        fields.imm = (instruction & 0xFF) * 4;
        fields.rn = 13; // SP
        fields.load_store_bit = (instruction & 0x0800) != 0;
        fields.byte_word = false;
        fields.type = fields.load_store_bit ? INST_T16_LDR_SP : INST_T16_STR_SP;
        return fields;
    }

    // Format 11: Load address (1010xxxx xxxxxxxx)
    if ((instruction & 0xF000) == 0xA000) {
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

    // Format 12: Add offset to Stack Pointer and PUSH/POP (1011xxxx xxxxxxxx)
    // But exclude hint instructions (10111111 xxxxxxxx) and BKPT (10111110 xxxxxxxx) which are handled separately
    if ((instruction & 0xF000) == 0xB000 && (instruction & 0xFF00) != 0xBF00 && (instruction & 0xFF00) != 0xBE00) {
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

        // Check for reverse instructions: 1011101000xxxxxx (0xBA00-0xBAFF)
        if ((instruction & 0xFF00) == 0xBA00) {
            // REV instructions: REV, REV16, REVSH
            fields.rd = instruction & 0x7;
            fields.rm = (instruction >> 3) & 0x7;
            uint32_t op = (instruction >> 6) & 0x3;
            switch (op) {
                case 0: // REV - 00
                    fields.type = INST_T16_REV;
                    break;
                case 1: // REV16 - 01
                    fields.type = INST_T16_REV16;
                    break;
                case 2: // Reserved
                    return fields; // Invalid encoding
                case 3: // REVSH - 11
                    fields.type = INST_T16_REVSH;
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

    // Format 13: Multiple load/store (1100xxxx xxxxxxxx)
    if ((instruction & 0xF000) == 0xC000) {
        fields.rn = (instruction >> 8) & 0x7;
        fields.reg_list = instruction & 0xFF;
        fields.load_store_bit = (instruction & 0x0800) != 0;
        fields.type = fields.load_store_bit ? INST_T16_LDMIA : INST_T16_STMIA;
        return fields;
    }

    // Format 14: Conditional branch (1101xxxx xxxxxxxx, but not 11011111 xxxxxxxx)
    if ((instruction & 0xF000) == 0xD000) {
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

    // Format 15: Unconditional branch (11100xxx xxxxxxxx)
    if ((instruction & 0xF800) == 0xE000) {
        fields.type = INST_T16_B;
        fields.cond = 0xE; // Always
        // Compute 12-bit signed offset: imm11:1 sign-extended
        uint32_t imm11 = instruction & 0x7FF;
        uint32_t off1 = (imm11 << 1);        // 12-bit value
        int32_t soff = (int32_t)(off1 << 20) >> 20; // sign-extend 12-bit to 32-bit
        fields.imm = (uint32_t)soff;
        return fields;
    }

    // Format 16: Long branch with link first half (1111xxxx xxxxxxxx)
    if ((instruction & 0xF000) == 0xF000) {
        fields.type = INST_T32_BL;
        fields.cond = 0xE;
        fields.alu_op = 1; // BL marker
        fields.imm = 0; // Will be fully decoded in decode_thumb32_instruction
        return fields;
    }

    // BKPT instruction (10111110xxxxxxxx)
    if ((instruction & 0xFF00) == 0xBE00) {
        fields.type = INST_T16_BKPT;
        fields.imm = instruction & 0xFF;  // Extract 8-bit immediate value
        return fields;
    }

    // HINT instructions (10111111xxxxxxxx) - NOP, WFI, WFE, etc.
    if ((instruction & 0xFF00) == 0xBF00) {
        uint32_t hint_op = instruction & 0xFF;

#if HAS_EXTENDED_HINTS
        // ARMv7-M extended hints
        switch (hint_op) {
            case 0x00: fields.type = INST_T16_NOP; break;
            case 0x10: fields.type = INST_T16_YIELD; break;
            case 0x20: fields.type = INST_T16_WFE; break;
            case 0x30: fields.type = INST_T16_WFI; break; 
            case 0x40: fields.type = INST_T16_SEV; break;
            default:   fields.type = INST_UNDEFINED; break; // NOP and others
        }
#endif
        return fields;
    }

    // Unknown instruction
    
    // Default: UNKNOWN instruction
    fields.type = INST_UNKNOWN;
    return fields;
}// ============================================================================
// T32 Instruction Decoding Helper Functions
// ============================================================================

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

// Helper function for T32 branch immediate calculation
static int32_t decode_t32_branch_immediate(uint32_t instruction, bool is_bl_or_unconditional) {
    uint32_t S  = (instruction >> 26) & 0x1;  // bit 26
    uint32_t J1 = (instruction >> 13) & 0x1;  // bit 13
    uint32_t J2 = (instruction >> 11) & 0x1;  // bit 11
    uint32_t imm11 = instruction & 0x07FF;    // bits 10-0

    if (is_bl_or_unconditional) {
        // BL or B.W: use 25-bit immediate
        uint32_t imm10 = (instruction >> 16) & 0x03FF; // bits 25-16
        uint32_t I1 = (~(J1 ^ S)) & 0x1;
        uint32_t I2 = (~(J2 ^ S)) & 0x1;
        uint32_t imm25 = (S << 24) | (I1 << 23) | (I2 << 22) | (imm10 << 12) | (imm11 << 1);
        int32_t signed_imm = (imm25 & 0x01000000) ? (int32_t)(imm25 | 0xFE000000) : (int32_t)imm25;
        return (signed_imm);
    } else {
        // B<cond>.W: use 20-bit immediate  
        uint32_t imm6  = (instruction >> 16) & 0x003F; // bits 21-16
        uint32_t imm20 = (S << 19) | (J2 << 18) | (J1 << 17) | (imm6 << 11) | (imm11 << 1);
        int32_t signed_imm = (imm20 & 0x00080000) ? (int32_t)(imm20 | 0xFFF00000) : (int32_t)imm20;
        return (signed_imm);
    }
}

#if SUPPORTS_ARMV7_M
// Helper function for data processing immediate instructions
static bool decode_t32_data_processing_imm(uint32_t instruction, InstructionFields& fields) {
    if ((instruction & 0xF8000000) != 0xF0000000) return false;
    
    uint32_t op = (instruction >> 21) & 0xF;
    bool s_bit = (instruction & 0x00100000) != 0;
    uint32_t rn = (instruction >> 16) & 0xF;
    uint32_t rd = (instruction >> 8) & 0xF;
    
    // Extract and decode modified immediate constant
    uint32_t i = (instruction >> 26) & 0x1;
    uint32_t imm3 = (instruction >> 12) & 0x7;
    uint32_t imm8 = instruction & 0xFF;
    // Build imm12 = i:imm3:imm8 and expand using ARM ThumbExpandImm
    uint32_t imm12 = (i << 11) | (imm3 << 8) | imm8;
    uint32_t imm_value = ThumbExpandImm(imm12);

    // Set common fields
    fields.rd = rd;
    fields.rn = rn;
    fields.imm = imm_value;
    fields.s_bit = s_bit;
    
    // Decode instruction type based on opcode
    static const InstructionType op_table[16] = {
        INST_T32_AND_IMM, INST_T32_BIC_IMM, INST_T32_ORR_IMM, INST_UNKNOWN,
        INST_T32_EOR_IMM, INST_UNKNOWN,     INST_UNKNOWN,     INST_UNKNOWN,
        INST_T32_ADD_IMM, INST_UNKNOWN,     INST_T32_ADC_IMM, INST_T32_SBC_IMM,
        INST_UNKNOWN,     INST_T32_SUB_IMM, INST_T32_RSB_IMM, INST_UNKNOWN
    };
    fields.type = op_table[op];
    
    // Handle compare instructions (S=1, Rd=0xF)
    if (s_bit && rd == 0xF) {
        switch (op) {
            case 0x0: fields.type = INST_T32_TST_IMM; break;
            case 0x4: fields.type = INST_T32_TEQ_IMM; break;
            case 0x8: fields.type = INST_T32_CMN_IMM; break;
            case 0xD: fields.type = INST_T32_CMP_IMM; break;
        }
    }
    return true;
}

// Helper function for data processing register instructions  
static bool decode_t32_data_processing_reg(uint32_t instruction, InstructionFields& fields) {
    if ((instruction & 0xFF008000) != 0xEA000000 && (instruction & 0xFF008000) != 0xEB000000) {
        return false;
    }
    
    uint32_t op = (instruction >> 21) & 0xF;
    bool s_bit = (instruction & 0x00100000) != 0;
    uint32_t rn = (instruction >> 16) & 0xF;
    uint32_t rd = (instruction >> 8) & 0xF;
    uint32_t rm = instruction & 0xF;
    uint32_t shift_type = (instruction >> 4) & 0x3;
    uint32_t imm3 = (instruction >> 12) & 0x7;
    uint32_t imm2 = (instruction >> 6) & 0x3;
    uint32_t shift_amount = (imm3 << 2) | imm2;
    
    // Set common fields
    fields.rn = rn;
    fields.rd = rd;
    fields.rm = rm;
    fields.shift_type = shift_type;
    fields.shift_amount = shift_amount;
    fields.s_bit = s_bit;
    
    // Decode instruction type with S bit consideration
    switch (op) {
        case 0x0: fields.type = s_bit ? INST_T32_ANDS_REG : INST_T32_AND_REG; break;
        case 0x1: fields.type = s_bit ? INST_T32_BICS_REG : INST_T32_BIC_REG; break;
        case 0x2: 
            if (rn == 0xF) {
                fields.type = s_bit ? INST_T32_MOVS_REG : INST_T32_MOV_REG;
                fields.rn = 0;
            } else {
                fields.type = s_bit ? INST_T32_ORRS_REG : INST_T32_ORR_REG;
            }
            break;
        case 0x3:
            if (rn == 0xF) {
                fields.type = s_bit ? INST_T32_MVNS_REG : INST_T32_MVN_REG;
                fields.rn = 0;
            } else {
                fields.type = s_bit ? INST_T32_ORNS_REG : INST_T32_ORN_REG;
            }
            break;
        case 0x4: fields.type = s_bit ? INST_T32_EORS_REG : INST_T32_EOR_REG; break;
        case 0x6: fields.type = INST_T32_PKH_REG; break;
        case 0x8: fields.type = s_bit ? INST_T32_ADDS_REG : INST_T32_ADD_REG; break;
        case 0xA: fields.type = s_bit ? INST_T32_ADCS_REG : INST_T32_ADC_REG; break;
        case 0xB: fields.type = s_bit ? INST_T32_SBCS_REG : INST_T32_SBC_REG; break;
        case 0xD: fields.type = s_bit ? INST_T32_SUBS_REG : INST_T32_SUB_REG; break;
        case 0xE: fields.type = s_bit ? INST_T32_RSBS_REG : INST_T32_RSB_REG; break;
        default: fields.type = INST_UNKNOWN; break;
    }
    
    // Compare instructions (S=1, Rd=0xF)
    if (s_bit && rd == 0xF) {
        switch (op) {
            case 0x0: fields.type = INST_T32_TST_REG; break;
            case 0x4: fields.type = INST_T32_TEQ_REG; break;
            case 0x8: fields.type = INST_T32_CMN_REG; break;
            case 0xD: fields.type = INST_T32_CMP_REG; break;
        }
    }
    return true;
}

// Helper function for system instructions
static bool decode_t32_system_instr(uint32_t instruction, InstructionFields& fields) {
    // CLREX instruction
    if (instruction == 0xF3BF8F2F) {
        fields.type = INST_T32_CLREX;
        return true;
    }
    
    // Memory barrier instructions
    if ((instruction & 0xFFF0FF00) == 0xF3B08F00) {
        uint32_t op = (instruction >> 4) & 0xF;
        uint32_t option = instruction & 0xF;
        
        switch (op) {
            case 0x4: fields.type = INST_T32_DSB; break;
            case 0x5: fields.type = INST_T32_DMB; break;
            case 0x6: fields.type = INST_T32_ISB; break;
            default: fields.type = INST_UNKNOWN; break;
        }
        fields.imm = option;
        return true;
    }
    
    // MSR instructions
    if ((instruction & 0xFFE0FF00) == 0xF3808800) {
        fields.type = INST_T32_MSR;
        fields.rn = (instruction >> 16) & 0xF;
        fields.imm = instruction & 0xFF;
        return true;
    }
    
    // MRS instructions
    if ((instruction & 0xFFFFF000) == 0xF3EF8000) {
        fields.type = INST_T32_MRS;
        fields.rd = (instruction >> 8) & 0xF;
        fields.imm = instruction & 0xFF;
        return true;
    }
    return false;
}

// T32 Multiply Instructions Helper Function
static bool decode_t32_multiply_instr(uint32_t instruction, InstructionFields& fields, uint32_t mask, uint32_t pattern, bool is_long = false) {
    if ((instruction & mask) == pattern) {
        if (is_long) {
            // Long multiply instructions (FBA0 pattern)
            uint32_t op = (instruction >> 4) & 0xF;
            fields.rn = (instruction >> 16) & 0xF;      // Rn (operand 1)
            fields.rd = (instruction >> 12) & 0xF;      // RdLo (result low)
            fields.rs = (instruction >> 8) & 0xF;       // RdHi (result high)
            fields.rm = instruction & 0xF;              // Rm (operand 2)
            
            switch (op) {
                case 0x0: fields.type = INST_T32_UMULL; break;
                case 0x1: fields.type = INST_T32_SMULL; break;
                case 0x2: fields.type = INST_T32_UMLAL; break;
                case 0x3: fields.type = INST_T32_SMLAL; break;
                default: fields.type = INST_UNKNOWN; break;
            }
        } else {
            // Regular multiply instructions (FB00 pattern)
            uint32_t op = (instruction >> 4) & 0xF;
            uint32_t ra = (instruction >> 12) & 0xF;
            
            if (op == 0x0) {
                fields.type = (ra == 0xF) ? INST_T32_MUL : INST_T32_MLA;
            } else if (op == 0x1) {
                fields.type = INST_T32_MLS;
            } else {
                fields.type = INST_UNKNOWN;
            }
            
            fields.rn = (instruction >> 16) & 0xF;      // Multiplicand
            fields.rs = ra;                             // Addend (Ra)
            fields.rd = (instruction >> 8) & 0xF;       // Destination
            fields.rm = instruction & 0xF;              // Multiplier
        }
        return true;
    }
    return false;
}

// T32 Bit Manipulation Instructions Helper Function
static bool decode_t32_bit_manip_instr(uint32_t instruction, InstructionFields& fields, uint32_t mask, uint32_t pattern, InstructionType type) {
    if ((instruction & mask) == pattern) {
        fields.type = type;
        fields.rm = (instruction >> 16) & 0xF;      // Source register
        fields.rd = (instruction >> 8) & 0xF;       // Destination register
        return true;
    }
    return false;
}

// T32 Bit Field Instructions Helper Function
static bool decode_t32_bitfield_instr(uint32_t instruction, InstructionFields& fields, uint32_t mask, uint32_t pattern, 
                                      InstructionType type, bool has_rn = true, bool is_extract = false) {
    if ((instruction & mask) == pattern) {
        fields.type = type;
        if (has_rn) {
            fields.rn = (instruction >> 16) & 0xF;  // Source
        }
        fields.rd = (instruction >> 8) & 0xF;       // Destination
        
        // Decode bit field parameters (lsb and width/msb)
        uint32_t imm3 = (instruction >> 12) & 0x7;
        uint32_t imm2 = (instruction >> 6) & 0x3;
        uint32_t lsb = (imm3 << 2) | imm2;
        uint32_t field_value = instruction & 0x1F;
        
        if (is_extract) {
            // For UBFX/SBFX: field_value is width-1
            uint32_t width = field_value + 1;
            fields.imm = (width << 8) | lsb;
        } else {
            // For BFI/BFC: field_value is msb
            fields.imm = (field_value << 8) | lsb;
        }
        return true;
    }
    return false;
}

// T32 Sign/Zero Extend Instructions Helper Function
static bool decode_t32_extend_instr(uint32_t instruction, InstructionFields& fields, uint32_t mask, uint32_t pattern, InstructionType type) {
    if ((instruction & mask) == pattern) {
        fields.type = type;
        fields.rn = instruction & 0xF;              // Source register (Rm)
        fields.rd = (instruction >> 8) & 0xF;       // Destination register
        return true;
    }
    return false;
}

// T32 Saturating Arithmetic Instructions Helper Function
static bool decode_t32_saturating_instr(uint32_t instruction, InstructionFields& fields, uint32_t mask, uint32_t pattern, InstructionType type) {
    if ((instruction & mask) == pattern) {
        fields.type = type;
        fields.rn = (instruction >> 16) & 0xF;      // Source register
        fields.rd = (instruction >> 8) & 0xF;       // Destination
        fields.imm = (((instruction >> 16) & 0x1F) << 16) | (instruction & 0x7F); // sat_imm:shift
        return true;
    }
    return false;
}

// T32 Load/Store Instructions Helper Function
static bool decode_t32_load_store_immediate(uint32_t instruction, InstructionFields& fields, uint32_t pattern_mask, uint32_t pattern_value, 
                                             InstructionType load_type, InstructionType store_type) {
    if ((instruction & pattern_mask) == pattern_value) {
        bool is_load = (instruction & 0x00100000) != 0;
        uint32_t rn = (instruction >> 16) & 0xF;
        uint32_t rt = (instruction >> 12) & 0xF;
        uint32_t imm12 = instruction & 0xFFF;
        
        // Skip PC-relative loads for non-PC-relative patterns
        if (rn != 15 || pattern_value == 0xF85F0000) {
            fields.rn = (pattern_value == 0xF85F0000) ? 15 : rn; // Special case for PC-relative
            fields.rd = rt;
            fields.imm = imm12;
            fields.load_store_bit = is_load;
            fields.type = is_load ? load_type : store_type;
            return true;
        }
    }
    return false;
}

// T32 Multiple Load/Store Instructions Helper Function  
static bool decode_t32_multiple_load_store(uint32_t instruction, InstructionFields& fields) {
    if ((instruction & 0xFF000000) != 0xE8000000 && (instruction & 0xFF000000) != 0xE9000000) {
        return false;
    }
    
    bool is_e9_series = (instruction & 0xFF000000) == 0xE9000000;
    uint32_t rn = (instruction >> 16) & 0xF;
    uint16_t reg_list = instruction & 0xFFFF;
    
    fields.rn = rn;
    fields.reg_list = reg_list;
    
    if (is_e9_series) {
        // E900-E92F series: STMDB
        fields.load_store_bit = 0;
        fields.type = INST_T32_STMDB;
    } else {
        // E800 series: determine based on bits [21:20]
        uint32_t op = (instruction >> 20) & 0x3;
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
                fields.type = INST_UNKNOWN;
                break;
        }
    }
    return true;
}

// T32 Table Branch Instructions Helper Function
static bool decode_t32_table_branch(uint32_t instruction, InstructionFields& fields) {
    if ((instruction & 0xFFF0FFE0) == 0xE8D0F000) {
        bool is_halfword = (instruction & 0x0010) != 0;
        fields.type = is_halfword ? INST_T32_TBH : INST_T32_TBB;
        fields.rn = (instruction >> 16) & 0xF;
        fields.rm = instruction & 0xF;
        return true;
    }
    return false;
}

// T32 Exclusive Access Instructions Helper Function
static bool decode_t32_exclusive_instr(uint32_t instruction, InstructionFields& fields, uint32_t mask, uint32_t pattern, InstructionType type,
                                       bool has_imm = false, bool has_rd_rt = false) {
    if ((instruction & mask) == pattern) {
        fields.type = type;
        fields.rn = (instruction >> 16) & 0xF;
        
        if (has_rd_rt) {
            fields.rm = (instruction >> 12) & 0xF;  // Status register (Rd)
            fields.rd = (instruction >> 8) & 0xF;   // Source register (Rt)
        } else {
            fields.rd = (instruction >> 12) & 0xF;  // Destination register
        }
        
        if (has_imm) {
            fields.imm = (instruction & 0xFF) << 2; // Offset (word-aligned)
        }
        return true;
    }
    return false;
}
#endif

InstructionFields Instruction::decode_thumb32_instruction(uint32_t instruction)
{
    InstructionFields fields = {};
    fields.opcode = instruction;
    fields.is_32bit = true;
    fields.cond = 0xE; // 默认 AL

    // 按 A5.3 表格精确分类: op1 (bits 28:27), op2 (bits 26:20), op (bit 15)
    uint32_t op1 = (instruction >> 27) & 0x3;    // bits 28:27 (2 bits)
    uint32_t op2 = (instruction >> 20) & 0x7F;   // bits 26:20 (7 bits) - 根据ARM手册表格
    uint32_t op = (instruction >> 15) & 0x1;     // bit 15

#if SUPPORTS_ARMV7_M
    if (op1 == 0x1) {
        // 01 00xx0xx x: Load/store multiple
        if ((op2 & 0x64) == 0x00) {
            // 按照ARM手册A5-16表格实现加载/存储多个寄存器指令
            uint32_t op_field = (instruction >> 23) & 0x3;    // bits 24:23 (op)
            uint32_t L = (instruction >> 20) & 0x1;           // bit 20 (L)
            uint32_t W = (instruction >> 21) & 0x1;           // bit 21 (W)
            uint32_t rn = (instruction >> 16) & 0xF;          // bits 19:16 (W.Rn)
            
            // 按照A5-16表格的op、L、W.Rn字段分类
            switch (op_field) {
                case 0x1: // 01
                    if (L == 0) {
                        // 01 0: Store Multiple (Increment After, Empty Ascending)
                        fields.type = INST_T32_STMIA;
                        fields.rn = rn;
                        fields.reg_list = instruction & 0xFFFF;  // register_list
                        fields.load_store_bit = 0;
                        fields.writeback = (W == 1);
                        return fields;
                    } else {
                        // 01 1
                        #if 0
                        if ((W == 1) && (rn == 0xD)) {
                            // 01 1 1.1101: Pop Multiple Registers from the stack
                            fields.type = INST_T16_POP;  // Use T16 POP for now
                            fields.rn = rn;
                            fields.reg_list = instruction & 0xFFFF;  // register_list
                            fields.load_store_bit = 1;
                            fields.writeback = true;
                            return fields;
                        } else
                        #endif
                        {
                            // 01 1 not(1.1101): Load Multiple (Increment After, Full Descending)
                            fields.type = INST_T32_LDMIA;
                            fields.rn = rn;
                            fields.reg_list = instruction & 0xFFFF;  // register_list
                            fields.load_store_bit = 1;
                            fields.writeback = (W == 1);
                            return fields;
                        }
                    }
                    break;
                case 0x2: // 10
                    if (L == 1) {
                        // 10 1: Load Multiple (Decrement Before, Empty Ascending)
                        fields.type = INST_T32_LDMDB;
                        fields.rn = rn;
                        fields.reg_list = instruction & 0xFFFF;  // register_list
                        fields.load_store_bit = 1;
                        fields.writeback = (W == 1);
                        return fields;
                    } else {
                        #if 0
                        // 10 0
                        if ((W == 1) && (rn == 0xD)) {
                            // 10 0 1.1101: Push Multiple Registers to the stack
                            fields.type = INST_T16_PUSH;  // Use T16 PUSH for now
                            fields.rn = rn;
                            fields.reg_list = instruction & 0xFFFF;  // register_list
                            fields.load_store_bit = 0;
                            fields.writeback = true;
                            return fields;
                        } else
                        #endif
                        {
                            // 10 0 not(1.1101): Store Multiple (Decrement Before, Full Descending)
                            fields.type = INST_T32_STMDB;
                            fields.rn = rn;
                            fields.reg_list = instruction & 0xFFFF;  // register_list
                            fields.load_store_bit = 0;
                            fields.writeback = (W == 1);
                            return fields;
                        }
                    }
                    break;
                default:
                    // op = 00 or 11 are not defined in the table
                    fields.type = INST_UNKNOWN;
                    return fields;
            }
        }

        // 01 00xx1xx x: Load/store dual or exclusive, table branch
        else if ((op2 & 0x64) == 0x04) {
            // 按照ARM手册A5-17表格实现加载/存储双字或独占访问、表分支指令
            uint32_t op1_field = (instruction >> 23) & 0x3;   // bits 24:23 (op1)
            uint32_t op2_field = (instruction >> 20) & 0x3;   // bits 21:20 (op2)
            uint32_t op3_field = (instruction >> 4) & 0xF;    // bits 7:4 (op3)
            uint32_t rn = (instruction >> 16) & 0xF;          // bits 19:16 (Rn)
            
            // 按照A5-17表格的op1、op2、op3字段分类
            switch (op1_field) {
                case 0x0: // 00
                    switch (op2_field) {
                        case 0x0: // 00 00 xxxx: Store Register Exclusive
#if HAS_EXCLUSIVE_ACCESS
                            fields.type = INST_T32_STREX;
                            fields.rn = rn;                           // Rn (base)
                            fields.rd = (instruction >> 8) & 0xF;    // Rd (status)
                            fields.rm = (instruction >> 12) & 0xF;   // Rt (data)
                            fields.imm = (instruction & 0xFF) << 2;  // imm8 << 2
                            fields.load_store_bit = 0;
                            return fields;
#endif
                            break;
                        case 0x1: // 00 01 xxxx: Load Register Exclusive
#if HAS_EXCLUSIVE_ACCESS
                            fields.type = INST_T32_LDREX;
                            fields.rn = rn;                           // Rn (base)
                            fields.rd = (instruction >> 12) & 0xF;   // Rt (data)
                            fields.imm = (instruction & 0xFF) << 2;  // imm8 << 2
                            fields.load_store_bit = 1;
                            return fields;
#endif
                            break;
                        case 0x2: // 00 10 xxxx: Store Register Dual
                            fields.type = INST_T32_STRD;
                            fields.rn = rn;                          // Rn (base)
                            fields.rd = (instruction >> 12) & 0xF;   // Rt (first register)
                            fields.rm = (instruction >> 8) & 0xF;    // Rt2 (second register)
                            fields.imm = (instruction & 0xFF) << 2;  // imm8 << 2
                            fields.pre_indexed = (instruction & 0x01000000) != 0;
                            fields.negative_offset = (instruction & 0x00800000) == 0;
                            fields.writeback = (instruction & 0x00200000) != 0;
                            fields.load_store_bit = 0;
                            return fields;
                        case 0x3: // 00 11 xxxx: Load Register Dual
                            fields.type = INST_T32_LDRD;
                            fields.rn = rn;                          // Rn (base)
                            fields.rd = (instruction >> 12) & 0xF;   // Rt (first register)
                            fields.rm = (instruction >> 8) & 0xF;    // Rt2 (second register)
                            fields.imm = (instruction & 0xFF) << 2;  // imm8 << 2
                            fields.pre_indexed = (instruction & 0x01000000) != 0;
                            fields.negative_offset = (instruction & 0x00800000) == 0;
                            fields.writeback = (instruction & 0x00200000) != 0;
                            fields.load_store_bit = 1;
                            return fields;
                        default:
                            fields.type = INST_UNKNOWN;
                            return fields;
                    }
                    break;
                case 0x1: // 01
                    switch (op2_field) {
                        case 0x0: // 01 00
                            switch (op3_field) {
                                case 0x4: // 01 00 0100: Store Register Exclusive Byte
#if HAS_EXCLUSIVE_ACCESS
                                    fields.type = INST_T32_STREXB;
                                    fields.rn = rn;                           // Rn (base)
                                    fields.rd = (instruction >> 0) & 0xF;    // Rd (status)
                                    fields.rm = (instruction >> 12) & 0xF;   // Rt (data)
                                    fields.load_store_bit = 0;
                                    return fields;
#endif
                                    break;
                                case 0x5: // 01 00 0101: Store Register Exclusive Halfword
#if HAS_EXCLUSIVE_ACCESS
                                    fields.type = INST_T32_STREXH;
                                    fields.rn = rn;                           // Rn (base)
                                    fields.rd = (instruction >> 0) & 0xF;    // Rd (status)
                                    fields.rm = (instruction >> 12) & 0xF;   // Rt (data)
                                    fields.load_store_bit = 0;
                                    return fields;
#endif
                                    break;
                                default:
                                    fields.type = INST_UNKNOWN;
                                    return fields;
                            }
                            break;
                        case 0x1: // 01 01
                            switch (op3_field) {
                                case 0x0: // 01 01 0000: Table Branch Byte
                                    fields.type = INST_T32_TBB;
                                    fields.rn = rn;                           // Rn (base)
                                    fields.rm = instruction & 0xF;            // Rm (offset)
                                    return fields;
                                case 0x1: // 01 01 0001: Table Branch Halfword
                                    fields.type = INST_T32_TBH;
                                    fields.rn = rn;                           // Rn (base)
                                    fields.rm = instruction & 0xF;            // Rm (offset)
                                    return fields;
                                case 0x4: // 01 01 0100: Load Register Exclusive Byte
#if HAS_EXCLUSIVE_ACCESS
                                    fields.type = INST_T32_LDREXB;
                                    fields.rn = rn;                           // Rn (base)
                                    fields.rd = (instruction >> 12) & 0xF;   // Rt (data)
                                    fields.load_store_bit = 1;
                                    return fields;
#endif
                                    break;
                                case 0x5: // 01 01 0101: Load Register Exclusive Halfword
#if HAS_EXCLUSIVE_ACCESS
                                    fields.type = INST_T32_LDREXH;
                                    fields.rn = rn;                           // Rn (base)
                                    fields.rd = (instruction >> 12) & 0xF;   // Rt (data)
                                    fields.load_store_bit = 1;
                                    return fields;
#endif
                                    break;
                                default:
                                    fields.type = INST_UNKNOWN;
                                    return fields;
                            }
                            break;
                        case 0x2: // 01 10 xxxx: Store Register Dual
                            fields.type = INST_T32_STRD;
                            fields.rn = rn;                          // Rn (base)
                            fields.rd = (instruction >> 12) & 0xF;   // Rt (first register)
                            fields.rm = (instruction >> 8) & 0xF;    // Rt2 (second register)
                            fields.imm = (instruction & 0xFF) << 2;  // imm8 << 2
                            fields.pre_indexed = (instruction & 0x01000000) != 0;
                            fields.negative_offset = (instruction & 0x00800000) == 0;
                            fields.writeback = (instruction & 0x00200000) != 0;
                            fields.load_store_bit = 0;
                            return fields;
                        case 0x3: // 01 11 xxxx: Load Register Dual
                            fields.type = INST_T32_LDRD;
                            fields.rn = rn;                          // Rn (base)
                            fields.rd = (instruction >> 12) & 0xF;   // Rt (first register)
                            fields.rm = (instruction >> 8) & 0xF;    // Rt2 (second register)
                            fields.imm = (instruction & 0xFF) << 2;  // imm8 << 2
                            fields.pre_indexed = (instruction & 0x01000000) != 0;
                            fields.negative_offset = (instruction & 0x00800000) == 0;
                            fields.writeback = (instruction & 0x00200000) != 0;
                            fields.load_store_bit = 1;
                            return fields;

                        default:
                            fields.type = INST_UNKNOWN;
                            return fields;
                    }
                    break;
                case 0x2: // 10 xx xxxx: Load Register Dual (literal)
                case 0x3: // 11 xx xxxx: Load Register Dual (literal)
                    if ((op2_field & 0x1) == 0x0){
                        // 1x x0 xxxx: STRD (immediate)
                        fields.type = INST_T32_STRD;
                        fields.rn = rn;                          // Rn (base)
                        fields.rd = (instruction >> 12) & 0xF;   // Rt (first register)
                        fields.rm = (instruction >> 8) & 0xF;    // Rt2 (second register)
                        fields.imm = (instruction & 0xFF) << 2;  // imm8 << 2
                        fields.pre_indexed = (instruction & 0x01000000) != 0;
                        fields.negative_offset = (instruction & 0x00800000) == 0;
                        fields.writeback = (instruction & 0x00200000) != 0;
                        fields.load_store_bit = 0;
                        return fields;
                    } else {
                        // 1x x1 xxxx: Load Register Dual (literal)
                        fields.type = INST_T32_LDRD;
                        fields.rn = rn;                          // Rn (base)
                        fields.rd = (instruction >> 12) & 0xF;   // Rt (first register)
                        fields.rm = (instruction >> 8) & 0xF;    // Rt2 (second register)
                        fields.imm = (instruction & 0xFF) << 2;  // imm8 << 2

                        fields.pre_indexed = (instruction & 0x01000000) != 0;
                        fields.negative_offset = (instruction & 0x00800000) == 0;
                        fields.writeback = (instruction & 0x00200000) != 0;
                        fields.load_store_bit = 1;
                        return fields;
                    }

                    break;
                default:
                    fields.type = INST_UNKNOWN;
                    return fields;
            }
            
            // Default fallback for unrecognized patterns
            fields.type = INST_UNKNOWN;
            return fields;
        }

        // 01 01xxxxx x: Data processing (shifted register)
        else if ((op2 & 0x60) == 0x20) {
            // 按照ARM手册A5-22表格实现数据处理(移位寄存器)指令
            uint32_t op_field = (instruction >> 21) & 0xF;    // bits 24:21 (op)
            uint32_t rn = (instruction >> 16) & 0xF;          // bits 19:16 (Rn)
            uint32_t rd = (instruction >> 8) & 0xF;           // bits 11:8 (Rd)
            uint32_t s_bit = (instruction >> 20) & 0x1;       // bit 20 (S)
            uint32_t rm = instruction & 0xF;                  // bits 3:0 (Rm)
            uint32_t shift_type = (instruction >> 4) & 0x3;   // bits 5:4 (type)
            uint32_t shift_imm = ((instruction >> 12) & 0x7) << 2 | ((instruction >> 6) & 0x3); // imm5
            
            // 设置通用字段
            fields.rd = rd;
            fields.rn = rn;
            fields.rm = rm;
            fields.s_bit = (s_bit == 1);
            fields.shift_type = shift_type;
            fields.shift_amount = shift_imm;
            
            // 按照A5-22表格的op、Rn、Rd、S字段分类
            switch (op_field) {
                case 0x0: // 0000: Bitwise AND
                    if (rd != 0xF) {
                        fields.type = INST_T32_AND_REG;  // Bitwise AND
                    } else {
                        // Rd = 1111, S = 0: UNPREDICTABLE
                        // Rd = 1111, S = 1: Test
                        if (s_bit == 1) {
                            fields.type = INST_T32_TST_REG;
                        } else {
                            fields.type = INST_UNKNOWN;  // UNPREDICTABLE
                        }
                    }
                    break;
                case 0x1: // 0001: Bitwise Bit Clear
                    fields.type = INST_T32_BIC_REG;
                    break;
                case 0x2: // 0010: Bitwise OR
                    if (rn != 0xF) {
                        fields.type = INST_T32_ORR_REG;  // Bitwise Inclusive OR
                    } else {
                        // Rn = 1111: Move register and immediate shifts
                        #if 0
                        switch (shift_type) {
                        case 0:
                            if(shift_imm == 0) {
                                fields.type = INST_T32_MOV_REG;  // MOV (register)
                            } else {
                                fields.type = INST_T32_LSL_IMM; // LSL
                            }
                            break;
                        case 1: fields.type = INST_T32_LSR_IMM; break;  // LSR
                        case 2: fields.type = INST_T32_ASR_IMM; break;  // ASR
                        case 3:
                            if(shift_imm == 0) {
                                //fields.type = INST_T32_RRX_IMM;  // RRX 
                            } else {
                                fields.type = INST_T32_ROR_IMM; // ROR
                            }
                            break;
                        default: fields.type = INST_UNKNOWN; break;
                        }
                        #endif
                        // This is always a MOV instruction, regardless of shift
                        fields.type = INST_T32_MOV_REG;  // MOV (register) with optional shift
                        fields.rn = 0;  // Clear Rn for move operations
                    }
                    break;
                case 0x3: // 0011: Bitwise OR NOT
                    if (rn != 0xF) {
                        fields.type = INST_T32_ORN_REG;  // Bitwise OR NOT
                    } else {
                        fields.type = INST_T32_MVN_REG;  // Bitwise NOT
                        fields.rn = 0;
                    }
                    break;
                case 0x4: // 0100: Bitwise Exclusive OR
                    if (rd != 0xF) {
                        fields.type = INST_T32_EOR_REG;  // Bitwise Exclusive OR
                    } else {
                        // Rd = 1111, S = 0: UNPREDICTABLE
                        // Rd = 1111, S = 1: Test Equivalence
                        if (s_bit == 1) {
                            fields.type = INST_T32_TEQ_REG;
                        } else {
                            fields.type = INST_UNKNOWN;  // UNPREDICTABLE
                        }
                    }
                    break;
                case 0x6: // 0110: Pack Halfword
#if SUPPORTS_ARMV7_M
                    fields.type = INST_T32_PKHBT;  // Pack halfword (bottom/top)
                    // Determine PKHBT vs PKHTB based on shift type
                    if (shift_type == 2) {  // ASR shift indicates PKHTB
                        fields.type = INST_T32_PKHTB;
                    }
#endif
                    break;
                case 0x8: // 1000: Add
                    if (rd != 0xF) {
                        fields.type = INST_T32_ADD_REG;  // Add
                    } else {
                        // Rd = 1111, S = 0: UNPREDICTABLE
                        // Rd = 1111, S = 1: Compare Negative
                        if (s_bit == 1) {
                            fields.type = INST_T32_CMN_REG;
                        } else {
                            fields.type = INST_UNKNOWN;  // UNPREDICTABLE
                        }
                    }
                    break;
                case 0xA: // 1010: Add with Carry
                    fields.type = INST_T32_ADC_REG;
                    break;
                case 0xB: // 1011: Subtract with Carry
                    fields.type = INST_T32_SBC_REG;
                    break;
                case 0xD: // 1101: Subtract
                    //if (rn != 0xF) {
                        if (rd != 0xF) {
                            fields.type = INST_T32_SUB_REG;  // Subtract
                        } else {
                            // Rd = 1111, S = 0: UNPREDICTABLE
                            // Rd = 1111, S = 1: Compare
                            if (s_bit == 1) {
                                fields.type = INST_T32_CMP_REG;
                            } else {
                                fields.type = INST_UNKNOWN;  // UNPREDICTABLE
                            }
                        }
                    //} else {
                    //    fields.type = INST_UNKNOWN;  // Rn = 1111 not defined
                    //}
                    break;
                case 0xE: // 1110: Reverse Subtract
                    fields.type = INST_T32_RSB_REG;
                    break;
                default:
                    fields.type = INST_UNKNOWN;
                    break;
            }
            return fields;
        }

        // 01 1xxxxxx x: Coprocessor instructions
        else if ((op2 & 0x40) == 0x40) {
            // 按照ARM手册A5-30表格实现协处理器指令
            uint32_t op1_field = (instruction >> 20) & 0x3F;  // bits 25:20 (op1)
            uint32_t coproc = (instruction >> 8) & 0xF;       // bits 11:8 (coproc)
            uint32_t op_field = (instruction >> 4) & 0x1;     // bit 4 (op)
            
            // 按照A5-30表格的op1、coproc、op字段分类
            if ((op1_field & 0x21) == 0x00) { // 0xxxx0: Store Coprocessor
                fields.type = INST_T32_STC;
                fields.rd = coproc;                           // 协处理器编号
                fields.rm = (instruction >> 12) & 0xF;       // CRd
                fields.rn = (instruction >> 16) & 0xF;       // Rn (base register)
                fields.imm = (instruction & 0xFF) << 2;      // imm8 << 2
                fields.load_store_bit = 0;
                // 处理P、U、W位
                bool p_bit = (instruction & 0x01000000) != 0;  // bit 24
                bool u_bit = (instruction & 0x00800000) != 0;  // bit 23
                bool w_bit = (instruction & 0x00200000) != 0;  // bit 21
                fields.addressing_mode = (p_bit ? 4 : 0) | (u_bit ? 2 : 0) | (w_bit ? 1 : 0);
                return fields;
            }
            else if ((op1_field & 0x21) == 0x01) { // 0xxxx1: Load Coprocessor
                if (((instruction >> 16) & 0xF) != 0xF) {
                    fields.type = INST_T32_LDC;               // LDC (immediate)
                } else {
                    fields.type = INST_T32_LDC_LIT;           // LDC (literal)
                }
                fields.rd = coproc;                           // 协处理器编号
                fields.rm = (instruction >> 12) & 0xF;       // CRd
                fields.rn = (instruction >> 16) & 0xF;       // Rn (base register)
                fields.imm = (instruction & 0xFF) << 2;      // imm8 << 2
                fields.load_store_bit = 1;
                // 处理P、U、W位
                bool p_bit = (instruction & 0x01000000) != 0;  // bit 24
                bool u_bit = (instruction & 0x00800000) != 0;  // bit 23
                bool w_bit = (instruction & 0x00200000) != 0;  // bit 21
                fields.addressing_mode = (p_bit ? 4 : 0) | (u_bit ? 2 : 0) | (w_bit ? 1 : 0);
                return fields;
            }
            else if (op1_field == 0x04) { // 000100: Move to Coprocessor from two ARM core registers
                fields.type = INST_T32_MCRR;
                fields.rd = coproc;                           // 协处理器编号
                fields.rm = (instruction >> 12) & 0xF;       // Rt (first ARM register)
                fields.rn = (instruction >> 16) & 0xF;       // Rt2 (second ARM register)
                fields.imm = instruction & 0xF;               // CRm
                fields.load_store_bit = 0;
                return fields;
            }
            else if (op1_field == 0x05) { // 000101: Move to two ARM core registers from Coprocessor
                fields.type = INST_T32_MRRC;
                fields.rd = coproc;                           // 协处理器编号
                fields.rm = (instruction >> 12) & 0xF;       // Rt (first ARM register)
                fields.rn = (instruction >> 16) & 0xF;       // Rt2 (second ARM register)
                fields.imm = instruction & 0xF;               // CRm
                fields.load_store_bit = 1;
                return fields;
            }
            else if ((op1_field & 0x30) == 0x20) { // 10xxxx: Coprocessor data operations and register transfers
                if (op_field == 0) {
                    // 10xxxx 0: Coprocessor data operations
                    fields.type = INST_T32_CDP;
                    fields.rd = coproc;                       // 协处理器编号
                    fields.rm = (instruction >> 12) & 0xF;   // CRd
                    fields.rn = (instruction >> 16) & 0xF;   // CRn
                    fields.imm = ((instruction >> 21) & 0x7) << 8 | (instruction & 0xF); // opc1:CRm
                    fields.shift_amount = (instruction >> 5) & 0x7; // opc2
                    return fields;
                }
                else if (op_field == 1) {
                    // 10xxxx 1: Move to/from Coprocessor from/to ARM core register
                    uint32_t L = (instruction >> 20) & 0x1;  // bit 20 (L)
                    if (L == 0) {
                        // 10xxx0 1: Move to Coprocessor from ARM core register
                        fields.type = INST_T32_MCR;
                        fields.rd = coproc;                       // 协处理器编号
                        fields.rm = (instruction >> 12) & 0xF;   // Rt (ARM register)
                        fields.rn = (instruction >> 16) & 0xF;   // CRn
                        fields.imm = ((instruction >> 21) & 0x7) << 8 | (instruction & 0xF); // opc1:CRm
                        fields.shift_amount = (instruction >> 5) & 0x7; // opc2
                        fields.load_store_bit = 0;
                        return fields;
                    } else {
                        // 10xxx1 1: Move to ARM core register from Coprocessor
                        fields.type = INST_T32_MRC;
                        fields.rd = coproc;                       // 协处理器编号
                        fields.rm = (instruction >> 12) & 0xF;   // Rt (ARM register)
                        fields.rn = (instruction >> 16) & 0xF;   // CRn
                        fields.imm = ((instruction >> 21) & 0x7) << 8 | (instruction & 0xF); // opc1:CRm
                        fields.shift_amount = (instruction >> 5) & 0x7; // opc2
                        fields.load_store_bit = 1;
                        return fields;
                    }
                }
            }
            
            // 对于Cortex-M，大部分协处理器指令不被支持
            // 仅系统协处理器(cp15)的部分指令被支持
            if (coproc == 15) {
                // 系统协处理器指令，可能被支持
                fields.type = INST_T32_SYSTEM_CP;
                return fields;
            } else {
                // 其他协处理器在Cortex-M中不被支持
                fields.type = INST_UNDEFINED;
                return fields;
            }
        }
    }
    
    else
#endif
    if (op1 == 0x2) {
#if SUPPORTS_ARMV7_M
        // 10 x0xxxxx 0: Data processing (modified immediate)
        if (((op2 & 0x20) == 0x00) && (op == 0)) {
            // 按照ARM手册A5-10表格实现32位修改立即数数据处理指令
            uint32_t op_field = (instruction >> 21) & 0xF;  // bits 24:21
            uint32_t rn = (instruction >> 16) & 0xF;        // bits 19:16
            uint32_t rd = (instruction >> 8) & 0xF;         // bits 11:8
            bool s_bit = (instruction & 0x00100000) != 0;   // bit 20
            
            // 提取修改立即数
            uint32_t i = (instruction >> 26) & 0x1;
            uint32_t imm3 = (instruction >> 12) & 0x7;
            uint32_t imm8 = instruction & 0xFF;
            uint32_t imm_value = decode_t32_modified_immediate(i, imm3, imm8);
            
            // 设置通用字段
            fields.rd = rd;
            fields.rn = rn;
            fields.imm = imm_value;
            fields.s_bit = s_bit;
            
            // 按照A5-10表格的op字段分类
            switch (op_field) {
                case 0x0: // 0000x
                    if (rd != 0xF) {
                        fields.type = INST_T32_AND_IMM;  // Bitwise AND
                    } else {
                        fields.type = INST_T32_TST_IMM;  // Test (when Rn=1111)
                    }
                    break;
                case 0x1: // 0001x
                    fields.type = INST_T32_BIC_IMM;  // Bitwise Clear
                    break;
                case 0x2: // 0010x
                    if (rn != 0xF) {
                        fields.type = INST_T32_ORR_IMM;  // Bitwise Inclusive OR
                    } else {
                        fields.type = INST_T32_MOV_IMM;  // Move (when Rn=1111)
                        fields.rn = 0;
                    }
                    break;
                case 0x3: // 0011x
                    if (rn != 0xF) {
                        fields.type = INST_T32_ORN_IMM;  // Bitwise OR NOT
                    } else {
                        fields.type = INST_T32_MVN_IMM;  // Bitwise NOT (when Rn=1111)
                        fields.rn = 0;
                    }
                    break;
                case 0x4: // 0100x
                    if (rd != 0xF) {
                        fields.type = INST_T32_EOR_IMM;  // Bitwise Exclusive OR
                    } else {
                        fields.type = INST_T32_TEQ_IMM;  // Test Equivalence (when Rn=1111)
                    }
                    break;
                case 0x8: // 1000x
                    if (rd != 0xF) {
                        fields.type = INST_T32_ADD_IMM;  // Add
                    } else {
                        fields.type = INST_T32_CMN_IMM;  // Compare Negative (when Rn=1111)
                    }
                    break;
                case 0xA: // 1010x
                    fields.type = INST_T32_ADC_IMM;  // Add with Carry
                    break;
                case 0xB: // 1011x
                    fields.type = INST_T32_SBC_IMM;  // Subtract with Carry
                    break;
                case 0xD: // 1101x
                    if (rd != 0xF) {
                        fields.type = INST_T32_SUB_IMM;  // Subtract
                    } else {
                        fields.type = INST_T32_CMP_IMM;  // Compare (when Rn=1111)
                    }
                    break;
                case 0xE: // 1110x
                    fields.type = INST_T32_RSB_IMM;  // Reverse Subtract
                    break;
                default:
                    fields.type = INST_UNKNOWN;
                    break;
            }
            return fields;
        }
        // 10 x1xxxxx 0: Data processing (plain binary immediate)
        else if (((op2 & 0x20) == 0x20) && (op == 0)) {
            // 按照ARM手册A5-12表格实现32位未修改立即数数据处理指令
            uint32_t op_field = (instruction >> 20) & 0x1F;  // bits 24:20 (5 bits)
            fields.rn = (instruction >> 16) & 0xF;         // bits 19:16
            fields.rd = (instruction >> 8) & 0xF;          // bits 11:8
            
            // 按照A5-12表格的op字段分类
            switch (op_field) {
                case 0x00: { // 00000: Add Wide, 12-bit
                    fields.type = INST_T32_ADDW;
                    // 提取12位立即数: i:imm3:imm8
                    uint32_t i = (instruction >> 26) & 0x1;
                    uint32_t imm3 = (instruction >> 12) & 0x7;
                    uint32_t imm8 = instruction & 0xFF;
                    fields.imm = (i << 11) | (imm3 << 8) | imm8;
                    break;
                }
                case 0x04: { // 00100: Move Wide, 16-bit
                    fields.type = INST_T32_MOVW;
                    // 提取16位立即数: imm4:i:imm3:imm8
                    uint32_t imm4 = (instruction >> 16) & 0xF;
                    uint32_t i = (instruction >> 26) & 0x1;
                    uint32_t imm3 = (instruction >> 12) & 0x7;
                    uint32_t imm8 = instruction & 0xFF;
                    fields.imm = (imm4 << 12) | (i << 11) | (imm3 << 8) | imm8;
                    break;
                }
                case 0x0A: { // 01010: Subtract Wide, 12-bit
                    fields.type = INST_T32_SUBW;
                    // 提取12位立即数: i:imm3:imm8
                    uint32_t i = (instruction >> 26) & 0x1;
                    uint32_t imm3 = (instruction >> 12) & 0x7;
                    uint32_t imm8 = instruction & 0xFF;
                    fields.imm = (i << 11) | (imm3 << 8) | imm8;
                    break;
                }
                case 0x0C: { // 01100: Move Top, 16-bit
                    fields.type = INST_T32_MOVT;
                    // 提取16位立即数: imm4:i:imm3:imm8
                    uint32_t imm4 = (instruction >> 16) & 0xF;
                    uint32_t i = (instruction >> 26) & 0x1;
                    uint32_t imm3 = (instruction >> 12) & 0x7;
                    uint32_t imm8 = instruction & 0xFF;
                    fields.imm = (imm4 << 12) | (i << 11) | (imm3 << 8) | imm8;
                    break;
                }
                case 0x10: { // 10000: Signed Saturate
                    fields.type = INST_T32_SSAT;
                    // 提取饱和位数和移位量
                    uint32_t sat_imm = (instruction & 0x1F) + 1;  // saturate_to = sat_imm + 1
                    uint32_t shift_type = (instruction >> 21) & 0x1;       // sh
                    uint32_t shift_imm = ((instruction >> 12) & 0x7) << 2 | ((instruction >> 6) & 0x3);
                    fields.imm = (sat_imm << 16) | (shift_type << 8) | shift_imm;
                    break;
                }
#if SUPPORTS_ARMV7_M
                case 0x12: { 
                    uint32_t ssat16 = (instruction >> 6) & 0x1C3; // bits 14:12 7:6

                    if (ssat16 == 0) {
                        fields.type = INST_T32_SSAT16;
                        uint32_t sat_imm = (instruction & 0xF) + 1;  // saturate_to = sat_imm + 1
                        fields.imm = sat_imm;
                    } else { // 10010: Signed Saturate, two 16-bit
                        fields.type = INST_T32_SSAT;
                        // 提取饱和位数和移位量
                        uint32_t sat_imm = (instruction & 0x1F) + 1;  // saturate_to = sat_imm + 1
                        uint32_t shift_type = (instruction >> 21) & 0x1;       // sh
                        uint32_t shift_imm = ((instruction >> 12) & 0x7) << 2 | ((instruction >> 6) & 0x3);
                        fields.imm = (sat_imm << 16) | (shift_type << 8) | shift_imm;
                    }
                    break;
                }
#endif
                case 0x14: { // 10100: Signed Bit Field Extract
                    fields.type = INST_T32_SBFX;
                    // 提取位域参数
                    uint32_t lsb = ((instruction >> 12) & 0x7) << 2 | ((instruction >> 6) & 0x3);
                    uint32_t widthm1 = instruction & 0x1F;  // width = widthm1 + 1
                    fields.imm = (lsb << 8) | (widthm1 + 1);
                    break;
                }
                case 0x16: { // 10110: Bit Field Insert
                    if (fields.rn != 0xF) {
                        fields.type = INST_T32_BFI;
                    } else {
                        fields.type = INST_T32_BFC;  // Bit Field Clear when Rn = 1111
                    }
                    // 提取位域参数
                    uint32_t lsb = ((instruction >> 12) & 0x7) << 2 | ((instruction >> 6) & 0x3);
                    uint32_t msb = instruction & 0x1F;
                    fields.imm = (lsb << 8) | (msb - lsb  + 1);  // width = msb - lsb + 1
                    break;
                }
                case 0x18: { // 11000: Unsigned Saturate
                    fields.type = INST_T32_USAT;
                    // 提取饱和位数和移位量
                    uint32_t sat_imm = instruction & 0x1F;       // saturate_to
                    uint32_t shift_type = (instruction >> 21) & 0x1;      // sh
                    uint32_t shift_imm = ((instruction >> 12) & 0x7) << 2 | ((instruction >> 6) & 0x3);
                    fields.imm = (sat_imm << 16) | (shift_type << 8) | shift_imm;
                    break;
                }
#if SUPPORTS_ARMV7_M
                case 0x1A: {
                    uint32_t usat16 = (instruction >> 6) & 0x1C3; // bits 14:12 7:6
                    if (usat16 == 0) {
                        fields.type = INST_T32_USAT16;
                        uint32_t sat_imm = instruction & 0xF;  // saturate_to
                        fields.imm = sat_imm;
                    } else { // 10010: Unsigned Saturate, two 16-bit
                        fields.type = INST_T32_USAT;
                        // 提取饱和位数和移位量
                        uint32_t sat_imm = instruction & 0x1F;    // saturate_to
                        uint32_t shift_type = (instruction >> 21) & 0x1;   // sh
                        uint32_t shift_imm = ((instruction >> 12) & 0x7) << 2 | ((instruction >> 6) & 0x3);
                        fields.imm = (sat_imm << 16) | (shift_type << 8) | shift_imm;
                    }
                    break;
                }
#endif
                case 0x1C: { // 11100: Unsigned Bit Field Extract
                    fields.type = INST_T32_UBFX;
                    // 提取位域参数
                    uint32_t lsb = ((instruction >> 12) & 0x7) << 2 | ((instruction >> 6) & 0x3);
                    uint32_t widthm1 = instruction & 0x1F;  // width = widthm1 + 1
                    fields.imm = (lsb << 8) | (widthm1 + 1);
                    break;
                }
                default:
                    fields.type = INST_UNKNOWN;
                    break;
            }
            return fields;
        }
        // 10 xxxxxxx 1: Branches and miscellaneous control
        else 
#endif
        if (op == 1) {
            // 按照ARM手册A5-13表格实现分支和杂项控制指令
            uint32_t op_field = (instruction >> 20) & 0x7F;  // bits 26:20
            uint32_t op1_field = (instruction >> 12) & 0x7;  // bits 14:12
            
            // 按照A5-13表格的op1字段分类
            //0x0
            if ((op1_field & 0x5) == 0x00) {
#if SUPPORTS_ARMV7_M
                // 0x0 not 111xxxx: Conditional branch
                if ((op_field & 0x38) != 0x38) {
                    fields.type = INST_T32_B_COND;
                    fields.cond = (instruction >> 22) & 0xF;  // condition field
                    int32_t simm = decode_t32_branch_immediate(instruction, false);
                    fields.imm = static_cast<uint32_t>(simm);
                    return fields;
                }
                // 0x0 011100x: Move to Special Register, MSR
                else 
#endif
                if ((op_field & 0x7E) == 0x38) {
                    fields.type = INST_T32_MSR;
                    fields.rn = (instruction >> 16) & 0xF;
                    fields.imm = instruction & 0xFF;
                    return fields;
                }
#if SUPPORTS_ARMV7_M
                // 0x0 0111010: Hint instructions
                else if (op_field == 0x3A) {
                    uint32_t hint_op = (instruction >> 4) & 0xF;  // bits 7:4
                    switch (hint_op) {
                        case 0x4: fields.type = INST_T32_SEV; break;
                        case 0x5: fields.type = INST_T32_SEVL; break;
                        case 0x6: fields.type = INST_T32_WFE; break;
                        default:  fields.type = INST_T32_NOP; break;
                    }
                    return fields;
                }
#endif
                // 0x0 0111011: Miscellaneous control instructions
                else if (op_field == 0x3B) {
#if SUPPORTS_ARMV7_M
                    // CLREX instruction check first (specific encoding)
                    if (instruction == 0xF3BF8F2F) {
                        fields.type = INST_T32_CLREX;
                        return fields;
                    }
#endif
                    uint32_t misc_op = (instruction >> 4) & 0xF;  // bits 7:4
                    switch (misc_op) {
                        case 0x4: fields.type = INST_T32_DSB; break;
                        case 0x5: fields.type = INST_T32_DMB; break;
                        case 0x6: fields.type = INST_T32_ISB; break;
                        default:  fields.type = INST_UNKNOWN; break;
                    }
                    fields.imm = instruction & 0xF;  // option field
                    return fields;
                }
                // 0x0 011111x: Move from Special Register, MRS
                else if ((op_field & 0x7E) == 0x3E) {
                    fields.type = INST_T32_MRS;
                    fields.rd = (instruction >> 8) & 0xF;
                    fields.imm = instruction & 0xFF;
                    return fields;
                }
            }
            // 010 0111111: Permanently UNDEFINED
            else if (op1_field == 0x2 && op_field == 0x7F) {
                fields.type = INST_UNKNOWN;  // UNDEFINED instruction
                return fields;
            }
#if SUPPORTS_ARMV7_M
            // 0x1 xxxxxxx: Branch
            else if ((op1_field & 0x5) == 0x1) {
                fields.type = INST_T32_B;
                int32_t simm = decode_t32_branch_immediate(instruction, true);
                fields.imm = static_cast<uint32_t>(simm);
                return fields;
            }
#endif
            // 1x1 xxxxxxx: Branch with Link
            else if ((op1_field & 0x5) == 0x5) {
                fields.type = INST_T32_BL;
                int32_t simm = decode_t32_branch_immediate(instruction, true);
                fields.imm = static_cast<uint32_t>(simm);
                fields.alu_op = 1;  // Mark as link instruction
                return fields;
            }
        }
    }
#if SUPPORTS_ARMV7_M
    // =====================================================================
    // op1 = 11: Store/Load single data item, data processing (register), multiply, coprocessor
    // =====================================================================
    else if (op1 == 0x3) {
        // 11 000xxx0 x: Store single data item  
        if ((op2 & 0x71) == 0x00) {
            // 按照ARM手册A5-21表格实现存储单个数据项指令
            uint32_t op1_field = (instruction >> 21) & 0x7;  // bits 23:21 (op1)
            uint32_t op2_field = (instruction >> 6) & 0x3F;  // bits 11:6 (op2)
            fields.rn = (instruction >> 16) & 0xF;
            fields.rd = (instruction >> 12) & 0xF;
            fields.load_store_bit = 0;
            fields.pre_indexed = (instruction & 0x400) != 0;
            fields.writeback = (instruction & 0x100) != 0;

            // 按照A5-21表格的op1和op2字段分类
            switch (op1_field) {
                case 0x0: // 000 xxxxxx: Store Register Byte
                    if ((op2_field & 0x20) == 0x20) {
                        // 000 1xxxxx: STRB (immediate)
                        fields.type = INST_T32_STRB_PRE_POST;
                        fields.negative_offset = (instruction & 0x200) == 0;
                        fields.imm = instruction & 0xFF;
                        return fields;
                    } else {
                        // 000 0xxxxx: STRB (register)
                        fields.type = INST_T32_STRB_REG;
                        fields.rm = instruction & 0xF;
                        fields.shift_amount = (instruction >> 4) & 0x3;
                        return fields;
                    }
                    break;
                case 0x1: // 001 xxxxxx: Store Register Halfword
                    if ((op2_field & 0x20) == 0x20) {
                        // 001 1xxxxx: STRH (immediate)
                        fields.type = INST_T32_STRH_PRE_POST;
                        fields.negative_offset = (instruction & 0x200) == 0;
                        fields.imm = instruction & 0xFF;
                        return fields;
                    } else {
                        // 001 0xxxxx: STRH (register)
                        fields.type = INST_T32_STRH_REG;
                        fields.rm = instruction & 0xF;
                        fields.shift_amount = (instruction >> 4) & 0x3;
                        return fields;
                    }
                    break;
                case 0x2: // 010 xxxxxx: Store Register
                    if ((op2_field & 0x20) == 0x20) {
                        // 010 1xxxxx: STR (immediate)
                        fields.type = INST_T32_STR_PRE_POST;
                        fields.negative_offset = (instruction & 0x200) == 0;
                        fields.imm = instruction & 0xFF;
                        return fields;
                    } else {
                        // 010 0xxxxx: STR (register)
                        fields.type = INST_T32_STR_REG;
                        fields.rm = instruction & 0xF;
                        fields.shift_amount = (instruction >> 4) & 0x3;
                        return fields;
                    }
                    break;
                case 0x4: // 100 xxxxxx: Store Register Byte
                    // 100 xxxxxx: STRB (immediate)
                    fields.type = INST_T32_STRB_IMM;
                    fields.imm = instruction & 0xFFF;
                    return fields;
                case 0x5: // 101 xxxxxx: Store Register Halfword
                    // 101 xxxxxx: STRH (immediate)
                    fields.type = INST_T32_STRH_IMM;
                    fields.imm = instruction & 0xFFF;
                    return fields;
                case 0x6: // 110 xxxxxx: Store Register
                    // 110 xxxxxx: STR (immediate)
                    fields.type = INST_T32_STR_IMM;
                    fields.imm = instruction & 0xFFF;
                    return fields;
                default:
                    // Other op1 values are UNDEFINED or reserved
                    fields.type = INST_UNKNOWN;
                    return fields;
            }
        }

        // 11 00xx001 x: Load byte, memory hints
        else if (((op2 & 0x67) == 0x01)) {
            // 按照ARM手册A5-20表格实现加载字节和内存提示指令
            uint32_t op1_field = (instruction >> 23) & 0x3;   // bits 24:23(op1)
            uint32_t op2_field = (instruction >> 6) & 0x3F;   // bits 11:6 (op2)
            fields.rn = (instruction >> 16) & 0xF;          // bits 19:16 (Rn)
            fields.rd = (instruction >> 12) & 0xF;          // bits 15:12 (Rt)
            
            // 按照A5-20表格的op1、op2、Rn、Rt字段分类
            if (op1_field == 0x0) {
                // 00 xxxxxx not 1111 not 1111:
                if (fields.rn != 0xF && fields.rd != 0xF) {
                    if (((op2_field & 0x24) == 0x24) 
                        // 0x 1xx1xx not 1111 not 1111: LDRB (immediate)
                        || ((op2_field & 0x3C) == 0x30)
                        // 0x 1100xx not 1111 not 1111: LDRB (immediate)
                        || ((op2_field & 0x3C) == 0x38)) {
                        // 0x 1110xx not 1111 not 1111: Load Register Byte Unprivileged LDRBT
                        fields.type = INST_T32_LDRB_PRE_POST;
                        fields.imm = instruction & 0xFF; //8 bit
                        fields.pre_indexed = (instruction & 0x400) != 0;
                        fields.negative_offset = (instruction & 0x200) == 0;
                        fields.writeback = (instruction & 0x100) != 0;
                        fields.load_store_bit = 1;
                        return fields;
                    } else if (op2_field == 0x00) {
                        // 0x 000000 not 1111 not 1111: LDRB (register)
                        fields.type = INST_T32_LDRB_REG;
                        fields.rm = instruction & 0xF;
                        fields.shift_amount = (instruction >> 4) & 0x3;
                        fields.load_store_bit = 1;
                        return fields;
                    }
                }
                // 0x xxxxxx 1111 not 1111: Load Register Byte
                else if (fields.rn == 0xF && fields.rd != 0xF) {
                    fields.type = INST_T32_LDRB_LIT;  // LDRB (literal)
                    fields.imm = instruction & 0xFFF; //12bit
                    fields.negative_offset = (instruction & 0x00800000) == 0;
                    fields.load_store_bit = 1;
                    return fields;
                }
                // Memory hints when Rt = 1111
                else if (fields.rn != 0xF && fields.rd == 0xF) {
                    if ((op2_field & 0x3C) == 0x30) {
                        // 00 1100xx not 1111 1111: PLD (immediate)
                        fields.type = INST_T32_PLD_IMM;
                        fields.imm = instruction & 0xFF; // 8-bit
                        return fields;
                    } else if (op2_field == 0) {
                        // 00 000000 not 1111 1111: PLD (register)
                        fields.type = INST_T32_PLD_REG;
                        fields.rm = instruction & 0xF;
                        fields.shift_amount = (instruction >> 4) & 0x3;
                        return fields;
                    }
                }
                else if (fields.rn == 0xF && fields.rd == 0xF) {
                    //0x xxxxxx 1111 1111: PLD (literal)
                    fields.type = INST_T32_PLD_LIT;
                    fields.imm = instruction & 0xFFF;
                    fields.negative_offset = (instruction & 0x00800000) == 0;
                    return fields;
                }
            }
            else if (op1_field == 0x1) {
                // 01 xxxxxx not 1111 not 1111: Load Register Byte
                if (fields.rn != 0xF && fields.rd != 0xF) {
                    // 1x 0xxxxx not 1111 not 1111: LDRB (immediate)
                    fields.type = INST_T32_LDRB_IMM;
                    fields.imm = instruction & 0xFFF;
                    fields.load_store_bit = 1;
                    return fields;
                }
                // 0x xxxxxx 1111 not 1111: Load Register Byte
                else if (fields.rn == 0xF && fields.rd != 0xF) {
                    fields.type = INST_T32_LDRSB_LIT;  // LDRB (literal)
                    fields.imm = instruction & 0xFFF;
                    fields.negative_offset = (instruction & 0x00800000) == 0;
                    fields.load_store_bit = 1;
                    return fields;
                }
                // 01 xxxxxx not 1111 1111: PLD (immediate)
                else if (fields.rn != 0xF && fields.rd == 0xF) {
                    fields.type = INST_T32_PLD_IMM;
                    fields.imm = instruction & 0xFFF;
                    return fields;
                }
                // 01 xxxxxx 1111 1111: PLD (literal)
                else if (fields.rn == 0xF && fields.rd == 0xF) {
                    fields.type = INST_T32_PLD_LIT;
                    fields.imm = instruction & 0xFFF;
                    fields.negative_offset = (instruction & 0x00800000) == 0;
                    return fields;
                }
            }
            else if (op1_field == 0x2) {
                // 10 xxxxxx not 1111 not 1111:
                if (fields.rn != 0xF && fields.rd != 0xF) {
                    if (((op2_field & 0x24) == 0x24) 
                        // 0x 1xx1xx not 1111 not 1111: LDRSB (immediate)
                        || ((op2_field & 0x3C) == 0x30)
                        // 0x 1100xx not 1111 not 1111: LDRSB (immediate)
                        || ((op2_field & 0x3C) == 0x38)) {
                        // 0x 1110xx not 1111 not 1111: Load Register Byte Unprivileged LDRSBT
                        fields.type = INST_T32_LDRSB_PRE_POST;
                        fields.imm = instruction & 0xFF; //8 bit
                        fields.pre_indexed = (instruction & 0x400) != 0;
                        fields.negative_offset = (instruction & 0x200) == 0;
                        fields.writeback = (instruction & 0x100) != 0;
                        fields.load_store_bit = 1;
                        return fields;
                    } else if (op2_field == 0x00) {
                        // 0x 000000 not 1111 not 1111: LDRSB (register)
                        fields.type = INST_T32_LDRSB_REG;
                        fields.rm = instruction & 0xF;
                        fields.shift_amount = (instruction >> 4) & 0x3;
                        fields.load_store_bit = 1;
                        return fields;
                    }
                }
                // 1x xxxxxx 1111 not 1111: Load Register Byte
                else if (fields.rn == 0xF && fields.rd != 0xF) {
                    fields.type = INST_T32_LDRSB_LIT;  // LDRSB (literal)
                    fields.imm = instruction & 0xFFF; //12bit
                    fields.negative_offset = (instruction & 0x00800000) == 0;
                    fields.load_store_bit = 1;
                    return fields;
                }
                // Memory hints when Rt = 1111
                else if (fields.rn != 0xF && fields.rd == 0xF) {
                    if ((op2_field & 0x3C) == 0x30) {
                        // 00 1100xx not 1111 1111: PLI (immediate)
                        fields.type = INST_T32_PLI_IMM;
                        fields.imm = instruction & 0xFF; // 8-bit
                        return fields;
                    } else if (op2_field == 0) {
                        // 00 000000 not 1111 1111: PLI (register)
                        fields.type = INST_T32_PLI_REG;
                        fields.rm = instruction & 0xF;
                        fields.shift_amount = (instruction >> 4) & 0x3;
                        return fields;
                    }
                }
                else if (fields.rn == 0xF && fields.rd == 0xF) {
                    //0x xxxxxx 1111 1111: PLD (literal)
                    fields.type = INST_T32_PLI_LIT;
                    fields.imm = instruction & 0xFFF;
                    fields.negative_offset = (instruction & 0x00800000) == 0;
                    return fields;
                }
            }
            else { //op1_field == 0x3
                // Load Register Signed Byte Unprivileged when specific conditions
                if (fields.rn != 0xF && fields.rd != 0xF) {
                    // 11 xxxxxx not 1111 not 1111: LDRSB (immediate) 
                    fields.type = INST_T32_LDRSB_IMM;
                    fields.imm = instruction & 0xFFF;  // 12-bit immediate
                    fields.load_store_bit = 1;
                    return fields;
                }
                else if (fields.rn == 0xF && fields.rd != 0xF) {
                    // 11 xxxxxx 1111 not 1111: ULDRSB (literal) 
                    fields.type = INST_T32_LDRSB_LIT;
                    fields.imm = instruction & 0xFFF;  // 12-bit immediate
                    return fields;
                } 
                else if (fields.rn != 0xF && fields.rd == 0xF) {
                    // 11 xxxxxx not 1111 1111: PLI (immediate)
                    fields.type = INST_T32_PLI_IMM;
                    fields.imm = instruction & 0xFFF;  // 12-bit immediate
                    return fields;
                }
                else if (fields.rn == 0xF && fields.rd == 0xF)
                {
                    // 11 xxxxxx 1111 1111: PLI (literal)
                    fields.type = INST_T32_PLI_LIT;
                    fields.imm = instruction & 0xFFF;  // 12-bit immediate
                    return fields;
                }
            }
        }

        // 11 00xx011 x: Load halfword, unallocated memory hints
        else if (((op2 & 0x67) == 0x03)) {
            // 按照ARM手册A5-19表格实现加载半字和内存提示指令
            // 按照ARM手册A5-20表格实现加载字节和内存提示指令
            uint32_t op1_field = (instruction >> 23) & 0x3;   // bits 24:23(op1)
            uint32_t op2_field = (instruction >> 6) & 0x3F;   // bits 11:6 (op2)
            fields.rn = (instruction >> 16) & 0xF;          // bits 19:16 (Rn)
            fields.rd = (instruction >> 12) & 0xF;          // bits 15:12 (Rt)
            
            // 按照A5-20表格的op1、op2、Rn、Rt字段分类
            if (op1_field == 0x0) {
                // 00 xxxxxx not 1111 not 1111:
                if (fields.rn != 0xF && fields.rd != 0xF) {
                    if (((op2_field & 0x24) == 0x24) 
                        // 0x 1xx1xx not 1111 not 1111: LDRH (immediate)
                        || ((op2_field & 0x3C) == 0x30)
                        // 0x 1100xx not 1111 not 1111: LDRH (immediate)
                        || ((op2_field & 0x3C) == 0x38)) {
                        // 0x 1110xx not 1111 not 1111: Unprivileged LDRHT
                        fields.type = INST_T32_LDRH_PRE_POST;
                        fields.imm = instruction & 0xFF; //8 bit
                        fields.pre_indexed = (instruction & 0x400) != 0;
                        fields.negative_offset = (instruction & 0x200) == 0;
                        fields.writeback = (instruction & 0x100) != 0;
                        fields.load_store_bit = 1;
                        return fields;
                    } else if (op2_field == 0x00) {
                        // 0x 000000 not 1111 not 1111: LDRH (register)
                        fields.type = INST_T32_LDRH_REG;
                        fields.rm = instruction & 0xF;
                        fields.shift_amount = (instruction >> 4) & 0x3;
                        fields.load_store_bit = 1;
                        return fields;
                    }
                }
                // 0x xxxxxx 1111 not 1111: Load Register Byte
                else if (fields.rn == 0xF && fields.rd != 0xF) {
                    fields.type = INST_T32_LDRH_LIT;  // LDRH (literal)
                    fields.imm = instruction & 0xFFF; //12bit
                    fields.negative_offset = (instruction & 0x00800000) == 0;
                    fields.load_store_bit = 1;
                    return fields;
                }
                // Memory hints when Rt = 1111
                else if (fields.rn != 0xF && fields.rd == 0xF) {
                    if ((op2_field & 0x3C) == 0x30) {
                        // 00 1100xx not 1111 1111: NOP (immediate)
                        fields.type = INST_T32_NOP;
                        fields.imm = instruction & 0xFF; // 8-bit
                        return fields;
                    } else if (op2_field == 0) {
                        // 00 000000 not 1111 1111: NOP (register)
                        fields.type = INST_T32_NOP;
                        fields.rm = instruction & 0xF;
                        fields.shift_amount = (instruction >> 4) & 0x3;
                        return fields;
                    }
                }
                else if (fields.rn == 0xF && fields.rd == 0xF) {
                    //0x xxxxxx 1111 1111: NOP (literal)
                    fields.type = INST_T32_NOP;
                    fields.imm = instruction & 0xFFF;
                    fields.negative_offset = (instruction & 0x00800000) == 0;
                    return fields;
                }
            }
            else if (op1_field == 0x1) {
                // 01 xxxxxx not 1111 not 1111: Load Register Byte
                if (fields.rn != 0xF && fields.rd != 0xF) {
                    // 1x 0xxxxx not 1111 not 1111: LDRH (immediate)
                    fields.type = INST_T32_LDRH_IMM;
                    fields.imm = instruction & 0xFFF;
                    fields.load_store_bit = 1;
                    return fields;
                }
                // 0x xxxxxx 1111 not 1111: Load Register Byte
                else if (fields.rn == 0xF && fields.rd != 0xF) {
                    fields.type = INST_T32_LDRSH_LIT;  // LDRH (literal)
                    fields.imm = instruction & 0xFFF;
                    fields.negative_offset = (instruction & 0x00800000) == 0;
                    fields.load_store_bit = 1;
                    return fields;
                }
                // 01 xxxxxx not 1111 1111: NOP (immediate)
                else if (fields.rn != 0xF && fields.rd == 0xF) {
                    fields.type = INST_T32_NOP;
                    fields.imm = instruction & 0xFFF;
                    return fields;
                }
                // 01 xxxxxx 1111 1111: NOP (literal)
                else if (fields.rn == 0xF && fields.rd == 0xF) {
                    fields.type = INST_T32_NOP;
                    fields.imm = instruction & 0xFFF;
                    fields.negative_offset = (instruction & 0x00800000) == 0;
                    return fields;
                }
            }
            else if (op1_field == 0x2) {
                // 10 xxxxxx not 1111 not 1111:
                if (fields.rn != 0xF && fields.rd != 0xF) {
                    if (((op2_field & 0x24) == 0x24) 
                        // 0x 1xx1xx not 1111 not 1111: LDRSH (immediate)
                        || ((op2_field & 0x3C) == 0x30)
                        // 0x 1100xx not 1111 not 1111: LDRSH (immediate)
                        || ((op2_field & 0x3C) == 0x38)) {
                        // 0x 1110xx not 1111 not 1111: Unprivileged LDRSHT
                        fields.type = INST_T32_LDRSH_PRE_POST;
                        fields.imm = instruction & 0xFF; //8 bit
                        fields.pre_indexed = (instruction & 0x400) != 0;
                        fields.negative_offset = (instruction & 0x200) == 0;
                        fields.writeback = (instruction & 0x100) != 0;
                        fields.load_store_bit = 1;
                        return fields;
                    } else if (op2_field == 0x00) {
                        // 0x 000000 not 1111 not 1111: LDRSH (register)
                        fields.type = INST_T32_LDRSH_REG;
                        fields.rm = instruction & 0xF;
                        fields.shift_amount = (instruction >> 4) & 0x3;
                        fields.load_store_bit = 1;
                        return fields;
                    }
                }
                // 1x xxxxxx 1111 not 1111: Load Register Byte
                else if (fields.rn == 0xF && fields.rd != 0xF) {
                    fields.type = INST_T32_LDRSH_LIT;  // LDRSH (literal)
                    fields.imm = instruction & 0xFFF; //12bit
                    fields.negative_offset = (instruction & 0x00800000) == 0;
                    fields.load_store_bit = 1;
                    return fields;
                }
                // Memory hints when Rt = 1111
                else if (fields.rn != 0xF && fields.rd == 0xF) {
                    if ((op2_field & 0x3C) == 0x30) {
                        // 00 1100xx not 1111 1111: NOP (immediate)
                        fields.type = INST_T32_NOP;
                        fields.imm = instruction & 0xFF; // 8-bit
                        return fields;
                    } else if (op2_field == 0) {
                        // 00 000000 not 1111 1111: NOP (register)
                        fields.type = INST_T32_NOP;
                        fields.rm = instruction & 0xF;
                        fields.shift_amount = (instruction >> 4) & 0x3;
                        return fields;
                    }
                }
                else if (fields.rn == 0xF && fields.rd == 0xF) {
                    //0x xxxxxx 1111 1111: NOP (literal)
                    fields.type = INST_T32_NOP;
                    fields.imm = instruction & 0xFFF;
                    fields.negative_offset = (instruction & 0x00800000) == 0;
                    return fields;
                }
            }
            else { //op1_field == 0x3
                // Load Register Signed Byte Unprivileged when specific conditions
                if (fields.rn != 0xF && fields.rd != 0xF) {
                    // 11 xxxxxx not 1111 not 1111: LDRSH (immediate) 
                    fields.type = INST_T32_LDRSH_IMM;
                    fields.imm = instruction & 0xFFF;  // 12-bit immediate
                    fields.load_store_bit = 1;
                    return fields;
                }
                else if (fields.rn == 0xF && fields.rd != 0xF) {
                    // 11 xxxxxx 1111 not 1111: LDRSH (literal) 
                    fields.type = INST_T32_LDRSH_LIT;
                    fields.imm = instruction & 0xFFF;  // 12-bit immediate
                    return fields;
                } 
                else if (fields.rn != 0xF && fields.rd == 0xF) {
                    // 11 xxxxxx not 1111 1111: NOP
                    fields.type = INST_T32_NOP;
                    fields.imm = instruction & 0xFFF;  // 12-bit immediate
                    return fields;
                }
                else if (fields.rn == 0xF && fields.rd == 0xF)
                {
                    // 11 xxxxxx 1111 1111: NOP
                    fields.type = INST_T32_NOP;
                    fields.imm = instruction & 0xFFF;  // 12-bit immediate
                    return fields;
                }
            }
        }
        
        // 11 00xx101 x: Load word
        else if (((op2 & 0x67) == 0x05)) {
            // 按照ARM手册A5-18表格实现加载字指令
            uint32_t op1_field = (instruction >> 23) & 0x3;   // bits 24:23 (op1)
            uint32_t op2_field = (instruction >> 6) & 0x3F;   // bits 11:6 (op2)
            fields.rn = (instruction >> 16) & 0xF;          // bits 19:16 (Rn)
            fields.rd = (instruction >> 12) & 0xF;
            fields.load_store_bit = 1;

            // 按照A5-18表格的op1、op2、Rn字段分类
            // 0x xxxxxx 1111: Load Register (literal)
            if (fields.rn == 0xF) {
                if ((op1_field & 0x2) == 0x0) {
                    fields.type = INST_T32_LDR_LIT;  // LDR (literal)
                    fields.negative_offset = (instruction & 0x00800000) == 0;
                    fields.imm = instruction & 0xFFF;
                    return fields;
                }
            }
            else {
                // 01 xxxxxx not 1111: LDR (immediate)
                if (op1_field == 0x1)
                {
                    fields.type = INST_T32_LDR_IMM;
                    fields.imm = instruction & 0xFFF;
                    fields.load_store_bit = 1;
                    return fields;
                }
                else if (
                // 00 1xx1xx not 1111: LDR (immediate) prepost
                    ((op1_field == 0x0) && ((op2_field & 0x24) == 0x24))
                // 00 1100xx not 1111: LDR (immediate) prepost
                    || ((op1_field == 0x0) && ((op2_field & 0x3C) == 0x30))
                ) {
                    fields.type = INST_T32_LDR_PRE_POST;
                    fields.imm = instruction & 0xFF;
                    fields.pre_indexed = (instruction & 0x400) != 0;
                    fields.negative_offset = (instruction & 0x200) == 0;
                    fields.writeback = (instruction & 0x100) != 0;
                    fields.load_store_bit = 1;
                    return fields;
                }
                // 00 000000 not 1111: LDR (register)
                else if (op2_field == 0)
                {
                    fields.type = INST_T32_LDR_REG;
                    fields.rm = instruction & 0xF;
                    fields.shift_amount = (instruction >> 4) & 0x3;
                    fields.load_store_bit = 1;
                    return fields;
                }
                // 00 1110xx not 1111: Load Register Unprivileged (LDRT)
                else if ((op2_field & 0x3C) == 0x38)
                {
                    fields.type = INST_T32_LDRT;
                    fields.imm = instruction & 0xFF;  // 8-bit immediate
                    fields.load_store_bit = 1;
                    return fields;
                }
            }
        }
        // 11 00xx111 x: UNDEFINED
        else if (((op2 & 0x67) == 0x07)) {
            // UNDEFINED
        }
        // 11 010xxxx x: Data processing (register)
        else if (((op2 & 0x70) == 0x20)) {
            // 按照ARM手册A5-24表格实现数据处理(寄存器)指令
            uint32_t op1_field = (instruction >> 20) & 0xF;   // bits 23:20 (op1)
            uint32_t op2_field = (instruction >> 4) & 0xF;    // bits 7:4 (op2)
            uint32_t rn = (instruction >> 16) & 0xF;          // bits 19:16 (Rn)
            
            // 按照A5-24表格的op1和op2字段分类
            switch (op1_field) {
                case 0x0: // 000x
                    if (op2_field == 0x0) {
                        // 0000: Logical Shift Left
                        fields.type = INST_T32_LSL_REG;
                        fields.rn = rn;                         // Rm (source)
                        fields.rd = (instruction >> 8) & 0xF;   // Rd
                        fields.rm = instruction & 0xF;          // Rs (shift amount)
                        fields.s_bit = (instruction & 0x00100000) != 0;
                        if (fields.s_bit) fields.type = INST_T32_LSLS_REG;
                        return fields;
                    }
                    else if (op2_field & 0x8 == 0x8)
                    {    
                        // 1xxx: Signed Extend and Add Halfword
                        if (rn != 0xF) {
                            fields.type = INST_T32_SXTAH;
                            fields.rn = rn;
                            fields.rd = (instruction >> 8) & 0xF;
                            fields.rm = instruction & 0xF;
                            fields.shift_amount = ((instruction >> 4) & 0x3) * 8;  // rotation
                            return fields;
                        } else {
                            // 1111: Signed Extend Halfword
                            fields.type = INST_T32_SXTH;
                            fields.rd = (instruction >> 8) & 0xF;
                            fields.rm = instruction & 0xF;
                            fields.shift_amount = ((instruction >> 4) & 0x3) * 8;  // rotation
                            return fields;
                        }
                    }
                    break;
                case 0x1: // 0001
                    if (op2_field == 0x0) {
                        fields.type = INST_T32_LSL_REG;
                        fields.rn = rn;                         // Rm (source)
                        fields.rd = (instruction >> 8) & 0xF;   // Rd
                        fields.rm = instruction & 0xF;          // Rs (shift amount)
                        fields.s_bit = (instruction & 0x00100000) != 0;
                        if (fields.s_bit) fields.type = INST_T32_LSLS_REG;
                        return fields;
                    }
                    else if (op2_field & 0x8 == 0x8)
                    {
                        // 1xxx: Unsigned Extend and Add Halfword
                        if (rn != 0xF) {
                            fields.type = INST_T32_UXTAH;
                            fields.rn = rn;
                            fields.rd = (instruction >> 8) & 0xF;
                            fields.rm = instruction & 0xF;
                            fields.shift_amount = ((instruction >> 4) & 0x3) * 8;  // rotation
                            return fields;
                        } else {
                            // 1111: Unsigned Extend Halfword
                            fields.type = INST_T32_UXTH;
                            fields.rd = (instruction >> 8) & 0xF;
                            fields.rm = instruction & 0xF;
                            fields.shift_amount = ((instruction >> 4) & 0x3) * 8;  // rotation
                            return fields;
                        }
                    }
                    break;
                case 0x2: // 0010
                    if (op2_field == 0) {
                        // 0001: Logical Shift Right
                        fields.type = INST_T32_LSR_REG;
                        fields.rn = rn;                         // Rm (source)
                        fields.rd = (instruction >> 8) & 0xF;   // Rd
                        fields.rm = instruction & 0xF;          // Rs (shift amount)
                        fields.s_bit = (instruction & 0x00100000) != 0;
                        if (fields.s_bit) fields.type = INST_T32_LSRS_REG;
                        return fields;
                    } else if (op2_field & 0x8 == 0x8) {
                        // 1xxx: Signed Extend and Add Byte 16
                        if (rn != 0xF) {
                            fields.type = INST_T32_SXTAB16;
                            fields.rn = rn;
                            fields.rd = (instruction >> 8) & 0xF;
                            fields.rm = instruction & 0xF;
                            fields.shift_amount = ((instruction >> 4) & 0x3) * 8;  // rotation
                            return fields;
                        } else {
                            // 1111: Signed Extend Byte 16
                            fields.type = INST_T32_SXTB16;
                            fields.rd = (instruction >> 8) & 0xF;
                            fields.rm = instruction & 0xF;
                            fields.shift_amount = ((instruction >> 4) & 0x3) * 8;  // rotation
                            return fields;
                        }
                    }
                    break;
                case 0x3: // 0011
                     if (op2_field == 0) {
                        // 0001: Logical Shift Right
                        fields.type = INST_T32_LSR_REG;
                        fields.rn = rn;                       // Rm (source)
                        fields.rd = (instruction >> 8) & 0xF;   // Rd
                        fields.rm = instruction & 0xF;          // Rs (shift amount)
                        fields.s_bit = (instruction & 0x00100000) != 0;
                        if (fields.s_bit) fields.type = INST_T32_LSRS_REG;
                        return fields;
                    } else if (op2_field & 0x8 == 0x8) {
                        // 1xxx: Unsigned Extend and Add Byte 16
                        if (rn != 0xF) {
                            fields.type = INST_T32_UXTAB16;
                            fields.rn = rn;
                            fields.rd = (instruction >> 8) & 0xF;
                            fields.rm = instruction & 0xF;
                            fields.shift_amount = ((instruction >> 4) & 0x3) * 8;  // rotation
                            return fields;
                        } else {
                            // 1111: Unsigned Extend Byte 16
                            fields.type = INST_T32_UXTB16;
                            fields.rd = (instruction >> 8) & 0xF;
                            fields.rm = instruction & 0xF;
                            fields.shift_amount = ((instruction >> 4) & 0x3) * 8;  // rotation
                            return fields;
                        }
                    }
                    break;
                case 0x4: // 0100
                     if (op2_field == 0) {
                        // 0010: Arithmetic Shift Right
                        fields.type = INST_T32_ASR_REG;
                        fields.rn = rn;                         // Rm (source)
                        fields.rd = (instruction >> 8) & 0xF;   // Rd
                        fields.rm = instruction & 0xF;          // Rs (shift amount)
                        fields.s_bit = (instruction & 0x00100000) != 0;
                        if (fields.s_bit) fields.type = INST_T32_ASRS_REG;
                        return fields;
                    } else if (op2_field & 0x8 == 0x8) {
                        // 1xxx: Signed Extend and Add Byte
                        if (rn != 0xF) {
                            fields.type = INST_T32_SXTAB;
                            fields.rn = rn;
                            fields.rd = (instruction >> 8) & 0xF;
                            fields.rm = instruction & 0xF;
                            fields.shift_amount = ((instruction >> 4) & 0x3) * 8;  // rotation
                            return fields;
                        } else {
                            // 1111: Signed Extend Byte
                            fields.type = INST_T32_SXTB;
                            fields.rd = (instruction >> 8) & 0xF;
                            fields.rm = instruction & 0xF;
                            fields.shift_amount = ((instruction >> 4) & 0x3) * 8;  // rotation
                            return fields;
                        }
                    }
                    break;
                case 0x5: // 0101
                     if (op2_field == 0) {
                        // 0010: Arithmetic Shift Right
                        fields.type = INST_T32_ASR_REG;
                        fields.rn = rn;                         // Rm (source)
                        fields.rd = (instruction >> 8) & 0xF;   // Rd
                        fields.rm = instruction & 0xF;          // Rs (shift amount)
                        fields.s_bit = (instruction & 0x00100000) != 0;
                        if (fields.s_bit) fields.type = INST_T32_ASRS_REG;
                        return fields;
                    } else if (op2_field & 0x8 == 0x8) {
                        // 1xxx: Unsigned Extend and Add Byte
                        if (rn != 0xF) {
                            fields.type = INST_T32_UXTAB;
                            fields.rn = rn;
                            fields.rd = (instruction >> 8) & 0xF;
                            fields.rm = instruction & 0xF;
                            fields.shift_amount = ((instruction >> 4) & 0x3) * 8;  // rotation
                            return fields;
                        } else {
                            // 1111: Unsigned Extend Byte
                            fields.type = INST_T32_UXTB;
                            fields.rd = (instruction >> 8) & 0xF;
                            fields.rm = instruction & 0xF;
                            fields.shift_amount = ((instruction >> 4) & 0x3) * 8;  // rotation
                            return fields;
                        }
                    }
                    break;
                case 0x6: // 0110
                case 0x7: // 0111
                    if (op2_field == 0) {
                        // 0011: Rotate Right
                        fields.type = INST_T32_ROR_REG;
                        fields.rn = rn;                         // Rm (source)
                        fields.rd = (instruction >> 8) & 0xF;   // Rd
                        fields.rm = instruction & 0xF;          // Rs (shift amount)
                        fields.s_bit = (instruction & 0x00100000) != 0;
                        if (fields.s_bit) fields.type = INST_T32_RORS_REG;
                        return fields;
                    }
                case 0x8: // 1000
                    if (op2_field == 8) {
                        // 1000: QADD
                        //fields.type = INST_T32_QADD;
                        fields.rn = rn;                         // Rm (source)
                        fields.rd = (instruction >> 8) & 0xF;   // Rd
                        fields.rm = instruction & 0xF;          // Rs (shift amount)
                        return fields;
                    }
                    if (op2_field == 9) {
                        // 1001: QDADD
                        //fields.type = INST_T32_QDADD;
                        fields.rn = rn;                         // Rm (source)
                        fields.rd = (instruction >> 8) & 0xF;   // Rd
                        fields.rm = instruction & 0xF;          // Rs (shift amount)
                        return fields;
                    }
                    if (op2_field == 10) {
                        // 1010: QSUB
                        //fields.type = INST_T32_QSUB;
                        fields.rn = rn;                         // Rm (source)
                        fields.rd = (instruction >> 8) & 0xF;   // Rd
                        fields.rm = instruction & 0xF;          // Rs (shift amount)
                        return fields;
                    }
                    if (op2_field == 11) {
                        // 1011: QDSUB
                        //fields.type = INST_T32_QDSUB;
                        fields.rn = rn;                         // Rm (source)
                        fields.rd = (instruction >> 8) & 0xF;   // Rd
                        fields.rm = instruction & 0xF;          // Rs (shift amount)
                        return fields;
                    }
                case 0x9: // 1001
                    if (op2_field == 8) {
                        // 1000: REV
                        fields.type = INST_T32_REV;
                        //rn must be same as rm
                        //fields.rn = rn;                       // Rm (source)
                        fields.rd = (instruction >> 8) & 0xF;   // Rd
                        fields.rm = instruction & 0xF;          // Rs (shift amount)
                        return fields;
                    }
                    if (op2_field == 9) {
                        // 1001: QDADD
                        fields.type = INST_T32_REV16;
                        //rn must be same as rm
                        //fields.rn = rn;                       // Rm (source)
                        fields.rd = (instruction >> 8) & 0xF;   // Rd
                        fields.rm = instruction & 0xF;          // Rs (shift amount)
                        return fields;
                    }
                    if (op2_field == 10) {
                        // 1010: RBIT
                        fields.type = INST_T32_RBIT;
                        //rn must be same as rm
                        //fields.rn = rn;                       // Rm (source)
                        fields.rd = (instruction >> 8) & 0xF;   // Rd
                        fields.rm = instruction & 0xF;          // Rs (shift amount)
                        return fields;
                    }
                    if (op2_field == 11) {
                        // 1011: REVSH
                        fields.type = INST_T32_REVSH;
                        //rn must be same as rm
                        //fields.rn = rn;                       // Rm (source)
                        fields.rd = (instruction >> 8) & 0xF;   // Rd
                        fields.rm = instruction & 0xF;          // Rs (shift amount)
                        return fields;
                    }
                case 0xA: // 1010
                    if (op2_field == 8) {
                        // 1000: SEL
                        //fields.type = INST_T32_SEL;
                        fields.rn = rn;                         // Rm (source)
                        fields.rd = (instruction >> 8) & 0xF;   // Rd
                        fields.rm = instruction & 0xF;          // Rs (shift amount)
                        return fields;
                    }
                case 0xB: // 1011
                    if (op2_field == 8) {
                        // 1000: CLZ
                        fields.type = INST_T32_CLZ;
                        //rn must be same as rm
                        //fields.rn = rn;                       // Rm (source)
                        fields.rd = (instruction >> 8) & 0xF;   // Rd
                        fields.rm = instruction & 0xF;          // Rs (shift amount)
                        return fields;
                    }
                default:
                    fields.type = INST_UNKNOWN;
                    return fields;
            }
        }
        // 11 0110xxx x: Multiply, and multiply accumulate - ARM manual direct parsing
        else if (((op2 & 0x78) == 0x30)) {
#if HAS_MULTIPLY_INSTRUCTIONS
            // 按照ARM手册A5-28表格实现乘法和乘法累加指令
            uint32_t op1_field = (instruction >> 20) & 0x7;   // bits 22:20 (op1)
            uint32_t op2_field = (instruction >> 4) & 0x3;    // bits 5:4 (op2)
            uint32_t ra = (instruction >> 12) & 0xF;          // bits 15:12 (Ra)
            uint32_t rn = (instruction >> 16) & 0xF;          // bits 19:16 (Rn)
            uint32_t rd = (instruction >> 8) & 0xF;           // bits 11:8 (Rd)
            uint32_t rm = instruction & 0xF;                  // bits 3:0 (Rm)
            
            // 按照A5-28表格的op1、op2、Ra字段分类
            switch (op1_field) {
                case 0x0: // 000
                    switch (op2_field) {
                        case 0x0: // 00
                            if (ra != 0xF) {
                                // 000 00 not 1111: Multiply Accumulate
                                fields.type = INST_T32_MLA;
                                fields.rd = rd;
                                fields.rn = rn;
                                fields.rm = rm;
                                fields.rs = ra;
                                return fields;
                            } else {
                                // 000 00 1111: Multiply
                                fields.type = INST_T32_MUL;
                                fields.rd = rd;
                                fields.rn = rn;
                                fields.rm = rm;
                                return fields;
                            }
                        case 0x1: // 01
                            // 000 01 -: Multiply and Subtract
                            fields.type = INST_T32_MLS;
                            fields.rd = rd;
                            fields.rn = rn;
                            fields.rm = rm;
                            fields.rs = ra;
                            return fields;
                        default:
                            fields.type = INST_UNKNOWN;
                            return fields;
                    }
                    break;
                case 0x1: // 001
                    if (op2_field == 0x0) {
                        if (ra != 0xF) {
                            // 001 - not 1111: Signed Multiply Accumulate, Halfwords
                            fields.type = INST_T32_SMLABB; // Base type, specific variant determined by op2 bits
                            fields.rd = rd;
                            fields.rn = rn;
                            fields.rm = rm;
                            fields.rs = ra;
                            fields.imm = (instruction >> 4) & 0x3; // M, N bits for halfword selection
                            return fields;
                        } else {
                            // 001 - 1111: Signed Multiply, Halfwords
                            fields.type = INST_T32_SMULBB; // Base type, specific variant determined by op2 bits
                            fields.rd = rd;
                            fields.rn = rn;
                            fields.rm = rm;
                            fields.imm = (instruction >> 4) & 0x3; // M, N bits for halfword selection
                            return fields;
                        }
                    }
                    break;
                case 0x2: // 010
                    if (op2_field == 0x0) {
                        if (ra != 0xF) {
                            // 010 0x not 1111: Signed Multiply Accumulate Dual
                            fields.type = INST_T32_SMLAD;
                            fields.rd = rd;
                            fields.rn = rn;
                            fields.rm = rm;
                            fields.rs = ra;
                            fields.imm = (instruction >> 4) & 0x1; // M bit
                            return fields;
                        } else {
                            // 010 0x 1111: Signed Dual Multiply Add
                            fields.type = INST_T32_SMUAD;
                            fields.rd = rd;
                            fields.rn = rn;
                            fields.rm = rm;
                            fields.imm = (instruction >> 4) & 0x1; // M bit
                            return fields;
                        }
                    }
                    break;
                case 0x3: // 011
                    if (op2_field == 0x0) {
                        if (ra != 0xF) {
                            // 011 0x not 1111: Signed Multiply Accumulate, Word by halfword
                            fields.type = INST_T32_SMLAWB;
                            fields.rd = rd;
                            fields.rn = rn;
                            fields.rm = rm;
                            fields.rs = ra;
                            fields.imm = (instruction >> 4) & 0x1; // M bit
                            return fields;
                        } else {
                            // 011 0x 1111: Signed Multiply, Word by halfword
                            fields.type = INST_T32_SMULWB;
                            fields.rd = rd;
                            fields.rn = rn;
                            fields.rm = rm;
                            fields.imm = (instruction >> 4) & 0x1; // M bit
                            return fields;
                        }
                    }
                    break;
                case 0x4: // 100
                    if (op2_field == 0x0) {
                        if (ra != 0xF) {
                            // 100 0x not 1111: Signed Multiply Subtract Dual
                            fields.type = INST_T32_SMLSD;
                            fields.rd = rd;
                            fields.rn = rn;
                            fields.rm = rm;
                            fields.rs = ra;
                            fields.imm = (instruction >> 4) & 0x1; // M bit
                            return fields;
                        } else {
                            // 100 0x 1111: Signed Dual Multiply Subtract
                            fields.type = INST_T32_SMUSD;
                            fields.rd = rd;
                            fields.rn = rn;
                            fields.rm = rm;
                            fields.imm = (instruction >> 4) & 0x1; // M bit
                            return fields;
                        }
                    }
                    break;
                case 0x5: // 101
                    if (op2_field == 0x0) {
                        if (ra != 0xF) {
                            // 101 0x not 1111: Signed Most Significant Word Multiply Accumulate
                            fields.type = INST_T32_SMMLA;
                            fields.rd = rd;
                            fields.rn = rn;
                            fields.rm = rm;
                            fields.rs = ra;
                            fields.imm = (instruction >> 4) & 0x1; // R bit (rounding)
                            return fields;
                        } else {
                            // 101 0x 1111: Signed Most Significant Word Multiply
                            fields.type = INST_T32_SMMUL;
                            fields.rd = rd;
                            fields.rn = rn;
                            fields.rm = rm;
                            fields.imm = (instruction >> 4) & 0x1; // R bit (rounding)
                            return fields;
                        }
                    }
                    break;
                case 0x6: // 110
                    if (op2_field == 0x0) {
                        // 110 0x -: Signed Most Significant Word Multiply Subtract
                        fields.type = INST_T32_SMMLS;
                        fields.rd = rd;
                        fields.rn = rn;
                        fields.rm = rm;
                        fields.rs = ra;
                        fields.imm = (instruction >> 4) & 0x1; // R bit (rounding)
                        return fields;
                    }
                    break;
                case 0x7: // 111
                    if (op2_field == 0x0) {
                        if (ra == 0xF) {
                            // 111 00 1111: Unsigned Sum of Absolute Differences, Accumulate
                            fields.type = INST_T32_USADA8;
                            fields.rd = rd;
                            fields.rn = rn;
                            fields.rm = rm;
                            fields.rs = ra;
                            return fields;
                        } else {
                            // 111 00 not 1111: Unsigned Sum of Absolute Differences
                            fields.type = INST_T32_USAD8;
                            fields.rd = rd;
                            fields.rn = rn;
                            fields.rm = rm;
                            return fields;
                        }
                    }
                    break;
                default:
                    fields.type = INST_UNKNOWN;
                    return fields;
            }
            
            // Default fallback for unrecognized patterns
            fields.type = INST_UNKNOWN;
            return fields;
#endif
        }

        // 11 0111xxx x: Long multiply, long multiply accumulate, and divide
        else if (((op2 & 0x78) == 0x38)) {
            // 按照ARM手册A5-29表格实现长乘法、长乘法累加和除法指令
            uint32_t op1_field = (instruction >> 20) & 0x7;   // bits 22:20 (op1)
            uint32_t op2_field = (instruction >> 4) & 0xF;    // bits 7:4 (op2)
            uint32_t rn = (instruction >> 16) & 0xF;          // bits 19:16 (Rn)
            uint32_t rdlo = (instruction >> 12) & 0xF;        // bits 15:12 (RdLo)
            uint32_t rdhi = (instruction >> 8) & 0xF;         // bits 11:8 (RdHi)
            uint32_t rm = instruction & 0xF;                  // bits 3:0 (Rm)
            
            // 按照A5-29表格的op1和op2字段分类
            switch (op1_field) {
                case 0x0: // 000
                    // 0000: Signed Multiply Long
                    if (op2_field == 0xF) {
                            fields.type = INST_T32_SMULL;
                            fields.rd = rdlo;   // RdLo
                            fields.rs = rdhi;   // RdHi
                            fields.rn = rn;     // Rn
                            fields.rm = rm;     // Rm
                            return fields;
                    }
                    break;
                case 0x1: // 001
                    if (op2_field == 0xF) {
                        // 001 1111: Signed Divide
                        fields.type = INST_T32_SDIV;
                        fields.rd = rdhi;   // Rd (result in RdHi field)
                        fields.rn = rn;     // Rn (dividend)
                        fields.rm = rm;     // Rm (divisor)
                        return fields;
                    }
                    break;
                case 0x2: // 010
                    // 010 0000: Unsigned Multiply Long
                    if (op2_field == 0x0) {
                        fields.type = INST_T32_UMULL;
                        fields.rd = rdlo;   // RdLo
                        fields.rs = rdhi;   // RdHi
                        fields.rn = rn;     // Rn
                        fields.rm = rm;     // Rm
                        return fields;
                    }
                    break;
                case 0x3: // 011
                    // 011 1111: Unsigned Divide
                    if (op2_field == 0xF) {
                        fields.type = INST_T32_UDIV;
                        fields.rd = rdhi;   // Rd (result in RdHi field)
                        fields.rn = rn;     // Rn (dividend)
                        fields.rm = rm;     // Rm (divisor)
                        return fields;
                    }
                    break;
                case 0x4: // 100
                    switch (op2_field) {
                        case 0x0: // 0000: Signed Multiply Accumulate Long
                            fields.type = INST_T32_SMLAL;
                            fields.rd = rdlo;   // RdLo
                            fields.rs = rdhi;   // RdHi
                            fields.rn = rn;     // Rn
                            fields.rm = rm;     // Rm
                            return fields;
                        case 0x8: // 10xx: Signed Multiply Accumulate Long, Halfwords (SMLALBB family)
                        case 0x9:
                        case 0xA:
                        case 0xB:
#if HAS_MULTIPLY_INSTRUCTIONS
                            fields.type = INST_T32_SMLALBB; // Base type, specific variant in imm field
                            fields.rd = rdlo;   // RdLo
                            fields.rs = rdhi;   // RdHi
                            fields.rn = rn;     // Rn
                            fields.rm = rm;     // Rm
                            fields.imm = op2_field & 0x3; // M, N bits for halfword selection
                            return fields;
#endif
                            break;
                        case 0xC: // 110x: Signed Multiply Accumulate Long Dual (SMLALD family)
                        case 0xD:
#if HAS_MULTIPLY_INSTRUCTIONS
                            fields.type = INST_T32_SMLALD;
                            fields.rd = rdlo;   // RdLo
                            fields.rs = rdhi;   // RdHi
                            fields.rn = rn;     // Rn
                            fields.rm = rm;     // Rm
                            fields.imm = op2_field & 0x1; // M bit
                            return fields;
#endif
                            break;
                        default:
                            fields.type = INST_UNKNOWN;
                            return fields;
                    }
                    break;
                case 0x5: // 101
                    if ((op2_field & 0xE) == 0xC) {
                        // 101 110x: Signed Multiply Subtract Long Dual (SMLSLD family)
#if HAS_MULTIPLY_INSTRUCTIONS
                        fields.type = INST_T32_SMLSLD;
                        fields.rd = rdlo;   // RdLo
                        fields.rs = rdhi;   // RdHi
                        fields.rn = rn;     // Rn
                        fields.rm = rm;     // Rm
                        fields.imm = op2_field & 0x1; // M bit
                        return fields;
#endif
                    }
                    break;
                case 0x6: // 110
                    switch (op2_field) {
                        case 0x0: // 0000: Unsigned Multiply Accumulate Long
                            fields.type = INST_T32_UMLAL;
                            fields.rd = rdlo;   // RdLo
                            fields.rs = rdhi;   // RdHi
                            fields.rn = rn;     // Rn
                            fields.rm = rm;     // Rm
                            return fields;
                        case 0x6: // 0110: Unsigned Multiply Accumulate Accumulate Long
#if HAS_MULTIPLY_INSTRUCTIONS
                            fields.type = INST_T32_UMAAL;
                            fields.rd = rdlo;   // RdLo
                            fields.rs = rdhi;   // RdHi
                            fields.rn = rn;     // Rn
                            fields.rm = rm;     // Rm
                            return fields;
#endif
                            break;
                        default:
                            fields.type = INST_UNKNOWN;
                            return fields;
                    }
                    break;
                default:
                    fields.type = INST_UNKNOWN;
                    return fields;
            }

            // Default fallback for unrecognized patterns
            fields.type = INST_UNKNOWN;
            return fields;
        }

        // 11 1xxxxxx x: Coprocessor instructions
        else if ((op2 & 0x40) == 0x40) {
            // 按照ARM手册A5-30表格实现协处理器指令
            uint32_t op1_field = (instruction >> 20) & 0x3F;  // bits 25:20 (op1)
            uint32_t op_field = (instruction >> 4) & 0x1;     // bit 4 (op)
            uint32_t coproc = (instruction >> 8) & 0xF;       // bits 11:8 (coproc)

            // 按照A5-30表格的op1、op、coproc字段分类
            if ((op1_field & 0x30) == 0x00) {
                // 0xxxxx: Store Coprocessor / Load Coprocessor
                if (op_field == 0) {
                    // 0xxxxx x xxxx: Store Coprocessor
#if HAS_COPROCESSOR_SUPPORT
                    fields.type = INST_T32_STC;
                    fields.coproc = coproc;
                    fields.rn = (instruction >> 16) & 0xF;     // Rn
                    fields.rd = (instruction >> 12) & 0xF;     // CRd
                    fields.imm = (instruction & 0xFF) << 2;    // imm8 << 2
                    fields.load_store_bit = 0;                 // Store
                    return fields;
#endif
                } else {
                    // 0xxxxx x xxxx: Load Coprocessor
#if HAS_COPROCESSOR_SUPPORT
                    fields.type = INST_T32_LDC;
                    fields.coproc = coproc;
                    fields.rn = (instruction >> 16) & 0xF;     // Rn
                    fields.rd = (instruction >> 12) & 0xF;     // CRd
                    fields.imm = (instruction & 0xFF) << 2;    // imm8 << 2
                    fields.load_store_bit = 1;                 // Load
                    return fields;
#endif
                }
            }
            else if ((op1_field & 0x3E) == 0x20) {
                // 100xxx: Move to Coprocessor from two Arm core registers
                if (op_field == 0) {
#if HAS_COPROCESSOR_SUPPORT
                    fields.type = INST_T32_MCRR;
                    fields.coproc = coproc;
                    fields.rn = (instruction >> 16) & 0xF;     // Rt
                    fields.rd = (instruction >> 12) & 0xF;     // Rt2
                    fields.rm = (instruction >> 4) & 0xF;      // opc1
                    fields.rs = instruction & 0xF;             // CRm
                    return fields;
#endif
                }
            }
            else if ((op1_field & 0x3E) == 0x22) {
                // 100xx1: Move to two Arm core registers from Coprocessor
                if (op_field == 0) {
#if HAS_COPROCESSOR_SUPPORT
                    fields.type = INST_T32_MRRC;
                    fields.coproc = coproc;
                    fields.rn = (instruction >> 16) & 0xF;     // Rt
                    fields.rd = (instruction >> 12) & 0xF;     // Rt2
                    fields.rm = (instruction >> 4) & 0xF;      // opc1
                    fields.rs = instruction & 0xF;             // CRm
                    return fields;
#endif
                }
            }
            else if ((op1_field & 0x30) == 0x20) {
                // 10xxxx: Coprocessor data operations
                if (op_field == 0) {
#if HAS_COPROCESSOR_SUPPORT
                    fields.type = INST_T32_CDP;
                    fields.coproc = coproc;
                    fields.rn = (instruction >> 16) & 0xF;     // CRn
                    fields.rd = (instruction >> 12) & 0xF;     // CRd
                    fields.rm = instruction & 0xF;             // CRm
                    fields.rs = (instruction >> 21) & 0xF;     // opc1
                    fields.imm = (instruction >> 5) & 0x7;     // opc2
                    return fields;
#endif
                }
            }
            else if ((op1_field & 0x31) == 0x20) {
                // 10xxx0: Move to Coprocessor from Arm core register
                if (op_field == 1) {
#if HAS_COPROCESSOR_SUPPORT
                    fields.type = INST_T32_MCR;
                    fields.coproc = coproc;
                    fields.rn = (instruction >> 12) & 0xF;     // Rt
                    fields.rd = (instruction >> 16) & 0xF;     // CRn
                    fields.rm = instruction & 0xF;             // CRm
                    fields.rs = (instruction >> 21) & 0x7;     // opc1
                    fields.imm = (instruction >> 5) & 0x7;     // opc2
                    return fields;
#endif
                }
            }
            else if ((op1_field & 0x31) == 0x21) {
                // 10xxx1: Move to Arm core register from Coprocessor
                if (op_field == 1) {
#if HAS_COPROCESSOR_SUPPORT
                    fields.type = INST_T32_MRC;
                    fields.coproc = coproc;
                    fields.rn = (instruction >> 12) & 0xF;     // Rt
                    fields.rd = (instruction >> 16) & 0xF;     // CRn
                    fields.rm = instruction & 0xF;             // CRm
                    fields.rs = (instruction >> 21) & 0x7;     // opc1
                    fields.imm = (instruction >> 5) & 0x7;     // opc2
                    return fields;
#endif
                }
            }

            // Coprocessor instructions not supported in Cortex-M or unrecognized pattern
            fields.type = INST_UNKNOWN;
            return fields;
        }
    }
#endif
    // Default fallback for unrecognized instructions
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
