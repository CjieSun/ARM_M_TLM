#include "Execute.h"
#include "CPU.h" // For calling request_svc on SVC execution
#include "ARM_CortexM_Config.h"
#include "Performance.h"
#include "Log.h"
#include <sstream>
#include <iomanip>
#include <cstring>

#if HAS_DSP_EXTENSIONS
// Helper for parallel add/sub lane operations (16-bit x2 or 8-bit x4)
static uint32_t saturate_signed(int32_t value, int bits, bool &sat) {
    int32_t maxv = (1 << (bits - 1)) - 1;
    int32_t minv = - (1 << (bits - 1));
    if (value > maxv) { sat = true; return (uint32_t)maxv & ((1u<<bits)-1); }
    if (value < minv) { sat = true; return (uint32_t)minv & ((1u<<bits)-1); }
    return (uint32_t)(value & ((1u<<bits)-1));
}
static uint32_t saturate_unsigned(int32_t value, int bits, bool &sat) {
    int32_t maxv = (1<<bits)-1;
    if (value < 0) { sat = true; return 0; }
    if (value > maxv) { sat = true; return (uint32_t)maxv; }
    return (uint32_t)value;
}

static uint32_t execute_parallel_addsub(Registers* regs, const InstructionFields &f) {
    uint32_t rn = regs->read_register(f.rn);
    uint32_t rm = regs->read_register(f.rm);
    bool is16 = false, is8 = false;
    bool signed_lane = false, saturating = false, halving = false, unsigned_saturating = false, unsigned_halving = false;
    enum Pattern { ADD16, SUB16, ASX, SAX, ADD8, SUB8 } pattern;

    switch (f.type) {
        case INST_T32_SADD16: signed_lane=true; is16=true; pattern=ADD16; break;
        case INST_T32_SSUB16: signed_lane=true; is16=true; pattern=SUB16; break;
        case INST_T32_SASX:   signed_lane=true; is16=true; pattern=ASX; break;
        case INST_T32_SSAX:   signed_lane=true; is16=true; pattern=SAX; break;
        case INST_T32_SADD8:  signed_lane=true; is8=true;  pattern=ADD8; break;
        case INST_T32_SSUB8:  signed_lane=true; is8=true;  pattern=SUB8; break;
        case INST_T32_QADD16: signed_lane=true; is16=true; pattern=ADD16; saturating=true; break;
        case INST_T32_QSUB16: signed_lane=true; is16=true; pattern=SUB16; saturating=true; break;
        case INST_T32_QASX:   signed_lane=true; is16=true; pattern=ASX; saturating=true; break;
        case INST_T32_QSAX:   signed_lane=true; is16=true; pattern=SAX; saturating=true; break;
        case INST_T32_QADD8:  signed_lane=true; is8=true;  pattern=ADD8; saturating=true; break;
        case INST_T32_QSUB8:  signed_lane=true; is8=true;  pattern=SUB8; saturating=true; break;
        case INST_T32_UADD16: is16=true; pattern=ADD16; break;
        case INST_T32_USUB16: is16=true; pattern=SUB16; break;
        case INST_T32_UASX:   is16=true; pattern=ASX; break;
        case INST_T32_USAX:   is16=true; pattern=SAX; break;
        case INST_T32_UADD8:  is8=true;  pattern=ADD8; break;
        case INST_T32_USUB8:  is8=true;  pattern=SUB8; break;
        case INST_T32_UQADD16: is16=true; pattern=ADD16; unsigned_saturating=true; break;
        case INST_T32_UQSUB16: is16=true; pattern=SUB16; unsigned_saturating=true; break;
        case INST_T32_UQASX:   is16=true; pattern=ASX; unsigned_saturating=true; break;
        case INST_T32_UQSAX:   is16=true; pattern=SAX; unsigned_saturating=true; break;
        case INST_T32_UQADD8:  is8=true;  pattern=ADD8; unsigned_saturating=true; break;
        case INST_T32_UQSUB8:  is8=true;  pattern=SUB8; unsigned_saturating=true; break;
        case INST_T32_SHADD16: signed_lane=true; is16=true; pattern=ADD16; halving=true; break;
        case INST_T32_SHSUB16: signed_lane=true; is16=true; pattern=SUB16; halving=true; break;
        case INST_T32_SHASX:   signed_lane=true; is16=true; pattern=ASX; halving=true; break;
        case INST_T32_SHSAX:   signed_lane=true; is16=true; pattern=SAX; halving=true; break;
        case INST_T32_SHADD8:  signed_lane=true; is8=true;  pattern=ADD8; halving=true; break;
        case INST_T32_SHSUB8:  signed_lane=true; is8=true;  pattern=SUB8; halving=true; break;
        case INST_T32_UHADD16: is16=true; pattern=ADD16; unsigned_halving=true; break;
        case INST_T32_UHSUB16: is16=true; pattern=SUB16; unsigned_halving=true; break;
        case INST_T32_UHASX:   is16=true; pattern=ASX; unsigned_halving=true; break;
        case INST_T32_UHSAX:   is16=true; pattern=SAX; unsigned_halving=true; break;
        case INST_T32_UHADD8:  is8=true;  pattern=ADD8; unsigned_halving=true; break;
        case INST_T32_UHSUB8:  is8=true;  pattern=SUB8; unsigned_halving=true; break;
        default: return 0; // not a parallel op
    }

    bool qsat = false; // any lane saturated sets Q flag
    uint32_t result = 0;
    if (is16) {
        int16_t rn_lo = rn & 0xFFFF; int16_t rn_hi = (rn >> 16) & 0xFFFF;
        int16_t rm_lo = rm & 0xFFFF; int16_t rm_hi = (rm >> 16) & 0xFFFF;
        auto u_rn_lo = (uint16_t)rn_lo; auto u_rn_hi=(uint16_t)rn_hi;
        auto u_rm_lo = (uint16_t)rm_lo; auto u_rm_hi=(uint16_t)rm_hi;
        uint16_t lanes[2];
        for (int lane=0; lane<2; ++lane) {
            int32_t a_s = lane==0? rn_lo: rn_hi;
            int32_t b_s = lane==0? rm_lo: rm_hi;
            uint32_t a_u = lane==0? u_rn_lo: u_rn_hi;
            uint32_t b_u = lane==0? u_rm_lo: u_rm_hi;
            int32_t val=0; uint32_t uval=0; bool sat=false;
            switch(pattern) {
                case ADD16: val = signed_lane? (a_s + b_s) : (int32_t)(a_u + b_u); uval = a_u + b_u; break;
                case SUB16: val = signed_lane? (a_s - b_s) : (int32_t)(a_u - b_u); uval = (a_u - b_u) & 0xFFFF; break;
                case ASX: // lower: a_s + b_s, upper: a_s - b_s with swap per spec; implement lane logic
                    if (lane==0) { val = signed_lane? (a_s + b_s) : (int32_t)(a_u + b_u); uval = a_u + b_u; }
                    else { val = signed_lane? (a_s - b_s) : (int32_t)(a_u - b_u); uval = (a_u - b_u) & 0xFFFF; }
                    break;
                case SAX:
                    if (lane==0) { val = signed_lane? (a_s - b_s) : (int32_t)(a_u - b_u); uval = (a_u - b_u) & 0xFFFF; }
                    else { val = signed_lane? (a_s + b_s) : (int32_t)(a_u + b_u); uval = a_u + b_u; }
                    break;
                case ADD8: case SUB8: break; // not used in 16-bit path
            }
            uint32_t outlane;
            if (saturating) {
                outlane = saturate_signed(val,16,sat);
            } else if (unsigned_saturating) {
                outlane = saturate_unsigned((int32_t)(uval),16,sat);
            } else if (halving) {
                outlane = (uint32_t)((val >> 1) & 0xFFFF);
            } else if (unsigned_halving) {
                outlane = (uval >> 1) & 0xFFFF;
            } else {
                outlane = signed_lane? (uint32_t)(val & 0xFFFF) : (uval & 0xFFFF);
            }
            if (sat) qsat = true;
            lanes[lane] = (uint16_t)outlane;
        }
        result = (uint32_t(lanes[1]) << 16) | lanes[0];
    } else if (is8) {
        uint8_t rn_b[4]; uint8_t rm_b[4];
        for (int i=0;i<4;++i) { rn_b[i]=(rn>>(i*8))&0xFF; rm_b[i]=(rm>>(i*8))&0xFF; }
        uint8_t outb[4];
        for (int i=0;i<4;++i) {
            int32_t a_s = (int8_t)rn_b[i]; int32_t b_s = (int8_t)rm_b[i];
            uint32_t a_u = rn_b[i]; uint32_t b_u = rm_b[i];
            int32_t val=0; uint32_t uval=0; bool sat=false;
            switch(pattern) {
                case ADD8: val = signed_lane? (a_s + b_s) : (int32_t)(a_u + b_u); uval = a_u + b_u; break;
                case SUB8: val = signed_lane? (a_s - b_s) : (int32_t)(a_u - b_u); uval = (a_u - b_u) & 0xFF; break;
                default: break; // other patterns not for 8-bit in this reduced mapping
            }
            uint32_t outlane;
            if (saturating) {
                outlane = saturate_signed(val,8,sat);
            } else if (unsigned_saturating) {
                outlane = saturate_unsigned((int32_t)uval,8,sat);
            } else if (halving) {
                outlane = (uint32_t)((val >> 1) & 0xFF);
            } else if (unsigned_halving) {
                outlane = (uval >> 1) & 0xFF;
            } else {
                outlane = signed_lane? (uint32_t)(val & 0xFF) : (uval & 0xFF);
            }
            if (sat) qsat = true;
            outb[i] = (uint8_t)outlane;
        }
        result = (uint32_t(outb[3])<<24)|(uint32_t(outb[2])<<16)|(uint32_t(outb[1])<<8)|outb[0];
    }
    if (qsat) regs->set_q_flag(true);
    regs->write_register(f.rd, result);
    return result;
}
#endif // HAS_DSP_EXTENSIONS

// CPU is included above for request_svc; we keep using its initiator socket type too

namespace {
// ===== Consolidated Formatting Utilities =====

// Format 32-bit values as 0xhhhhhhhh
static std::string hex32(uint32_t v) {
    std::ostringstream oss;
    oss << "0x" << std::hex << std::nouppercase << std::setw(8) << std::setfill('0') << v;
    return oss.str();
}

// Helper function to read register with PC adjustment for T32 instructions
static uint32_t read_register_with_pc_adjust(Registers* registers, uint8_t reg_num) {
    if (reg_num == 15) {
        // PC reads as current instruction address + 4 for T32 instructions
        return registers->read_register(reg_num) + 4;
    } else {
        return registers->read_register(reg_num);
    }
}

// Get register name with special names for sp, lr, pc
static std::string reg_name(uint8_t reg) {
    switch (reg) {
        case 10: return "sl"; // R10 is also called SL (Stack Limit)
        case 11: return "fp"; // R11 is also called FP (Frame Pointer)
        case 12: return "ip"; // R12 is also called IP (Intra-Procedure-call scratch register)
        case 13: return "sp";
        case 14: return "lr"; 
        case 15: return "pc";
        default: 
            return "r" + std::to_string(reg);
    }
}

// Format register list for multiple load/store instructions
static std::string format_reg_list(uint16_t reg_list) {
    std::ostringstream oss;
    oss << "{";
    bool first = true;
    
    for (int i = 0; i < 16; i++) {
        if (reg_list & (1 << i)) {
            if (!first) oss << ", ";
            oss << reg_name(i);
            first = false;
        }
    }
    oss << "}";
    return oss.str();
}

// Get condition code suffix for conditional instructions
static std::string cond_suffix(uint8_t cond) {
    switch (cond) {
        case 0x0: return "eq";
        case 0x1: return "ne";
        case 0x2: return "cs"; // or "hs"
        case 0x3: return "cc"; // or "lo"
        case 0x4: return "mi";
        case 0x5: return "pl";
        case 0x6: return "vs";
        case 0x7: return "vc";
        case 0x8: return "hi";
        case 0x9: return "ls";
        case 0xA: return "ge";
        case 0xB: return "lt";
        case 0xC: return "gt";
        case 0xD: return "le";
        case 0xE: return "";   // AL (always) - no suffix
        case 0xF: return "";   // Reserved
        default: return "";
    }
}

// Format shift operations for disassembly
static std::string format_shift(uint8_t shift_type, uint8_t shift_amount) {
    if (shift_amount == 0) {
        return "";
    }
    
    switch (shift_type) {
        case 0x0: return ", lsl #" + std::to_string(shift_amount);
        case 0x1: return ", lsr #" + std::to_string(shift_amount);
        case 0x2: return ", asr #" + std::to_string(shift_amount);
        case 0x3: return ", ror #" + std::to_string(shift_amount);
        default: return "";
    }
}

// Format memory addressing mode
static std::string format_address(uint8_t rn, int32_t offset, bool pre_indexed, bool writeback, bool negative_offset) {
    std::ostringstream oss;
    oss << "[" << reg_name(rn);
    
    // For pre-indexed addressing, include offset inside brackets
    if (pre_indexed && offset != 0) {
        oss << ", #" << (negative_offset ? -static_cast<int32_t>(offset) : static_cast<int32_t>(offset));
    }
    
    oss << "]";
    
    // Add writeback marker for pre-indexed, or offset for post-indexed
    if (writeback) {
        if (pre_indexed) {
            oss << "!";  // Pre-indexed with writeback: [rn, #offset]!
        } else {
            // Post-indexed: [rn], #offset
            if (offset != 0) {
                oss << ", #" << (negative_offset ? -static_cast<int32_t>(offset) : static_cast<int32_t>(offset));
            }
        }
    }
    
    return oss.str();
}

// Format T32 data processing instruction with register operands
static std::string format_t32_data_proc_reg(const std::string& mnemonic, bool s_flag, 
                                           uint8_t rd, uint8_t rn, uint8_t rm, 
                                           uint8_t shift_type, uint8_t shift_amount) {
    std::ostringstream oss;
    oss << mnemonic;
    if (s_flag) oss << "s";
    oss << ".w\t" << reg_name(rd) << ", " << reg_name(rn) << ", " << reg_name(rm);
    oss << format_shift(shift_type, shift_amount);
    return oss.str();
}

// Format T32 compare instruction with register operands  
static std::string format_t32_compare_reg(const std::string& mnemonic, 
                                         uint8_t rn, uint8_t rm, 
                                         uint8_t shift_type, uint8_t shift_amount) {
    std::ostringstream oss;
    oss << mnemonic << ".w\t" << reg_name(rn) << ", " << reg_name(rm);
    oss << format_shift(shift_type, shift_amount);
    return oss.str();
}

// Generate assembly string for instruction
static std::string format_instruction(const InstructionFields& fields) {
    std::ostringstream oss;
    
    switch (fields.type) {
        // T16 Data Processing Instructions
        case INST_T16_LSL_IMM:
            oss << "lsls\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm) << ", #" << (uint32_t)(fields.shift_amount);
            break;
        case INST_T16_LSR_IMM:
            oss << "lsrs\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm) << ", #" << (uint32_t)(fields.shift_amount);
            break;
        case INST_T16_ASR_IMM:
            oss << "asrs\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm) << ", #" << (uint32_t)(fields.shift_amount);
            break;
        case INST_T16_ADD_REG:
            oss << "adds\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_SUB_REG:
            oss << "subs\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_ADD_IMM3:
            oss << "adds\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", #" << fields.imm;
            break;
        case INST_T16_SUB_IMM3:
            oss << "subs\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", #" << fields.imm;
            break;
        case INST_T16_MOV_IMM:
            oss << "movs\t" << reg_name(fields.rd) << ", #" << fields.imm;
            break;
        case INST_T16_CMP_IMM:
            oss << "cmp\t" << reg_name(fields.rn) << ", #" << fields.imm;
            break;
        case INST_T16_ADD_IMM8:
            oss << "adds\t" << reg_name(fields.rd) << ", #" << fields.imm;
            break;
        case INST_T16_SUB_IMM8:
            oss << "sub\t" << reg_name(fields.rd) << ", #" << fields.imm;
            break;
        case INST_T16_AND:
            oss << "and\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_EOR:
            oss << "eor\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_LSL_REG:
            oss << "lsl\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_LSR_REG:
            oss << "lsr\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_ASR_REG:
            oss << "asr\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_ADC:
            oss << "adc\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_SBC:
            oss << "sbc\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_ROR:
            oss << "ror\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_TST:
            oss << "tst\t" << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_NEG:
            oss << "neg\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_CMP_REG:
            oss << "cmp\t" << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_CMN:
            oss << "cmn\t" << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_ORR:
            oss << "orr\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_MUL:
            oss << "mul\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_BIC:
            oss << "bic\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_MVN:
            oss << "mvn\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;

        // T16 Extend (SXTH, SXTB, UXTH, UXTB)
        case INST_T16_EXTEND: {
            const char* ext_names[4] = {"sxth", "sxtb", "uxth", "uxtb"};
            uint8_t op = fields.alu_op & 0x3;
            oss << ext_names[op] << "\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        }

        // T16 Reverse instructions
        case INST_T16_REV:
            oss << "rev\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_REV16:
            oss << "rev16\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_REVSH:
            oss << "revsh\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
            
        // T16 Hi Register Operations
        case INST_T16_ADD_HI:
            oss << "add\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_CMP_HI:
            oss << "cmp\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_MOV_HI:
            oss << "mov\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T16_BX:
            oss << "bx\t" << reg_name(fields.rm);
            break;
#if HAS_BLX_REGISTER
        case INST_T16_BLX:
            oss << "blx\t" << reg_name(fields.rm);
            break;
#endif

        // T16 Load/Store Instructions
        case INST_T16_LDR_PC:
            oss << "ldr\t" << reg_name(fields.rd) << ", [pc, #" << fields.imm << "]";
            break;
        case INST_T16_STR_REG:
            oss << "str\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", " << reg_name(fields.rm) << "]";
            break;
        case INST_T16_STRH_REG:
            oss << "strh\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", " << reg_name(fields.rm) << "]";
            break;
        case INST_T16_STRB_REG:
            oss << "strb\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", " << reg_name(fields.rm) << "]";
            break;
        case INST_T16_LDRSB_REG:
            oss << "ldrsb\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", " << reg_name(fields.rm) << "]";
            break;
        case INST_T16_LDR_REG:
            oss << "ldr\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", " << reg_name(fields.rm) << "]";
            break;
        case INST_T16_LDRH_REG:
            oss << "ldrh\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", " << reg_name(fields.rm) << "]";
            break;
        case INST_T16_LDRB_REG:
            oss << "ldrb\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", " << reg_name(fields.rm) << "]";
            break;
        case INST_T16_LDRSH_REG:
            oss << "ldrsh\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", " << reg_name(fields.rm) << "]";
            break;
        case INST_T16_STR_IMM:
            oss << "str\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", #" << fields.imm << "]";
            break;
        case INST_T16_LDR_IMM:
            oss << "ldr\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", #" << fields.imm << "]";
            break;
        case INST_T16_STRB_IMM:
            oss << "strb\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", #" << fields.imm << "]";
            break;
        case INST_T16_LDRB_IMM:
            oss << "ldrb\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", #" << fields.imm << "]";
            break;
        case INST_T16_STRH_IMM:
            oss << "strh\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", #" << fields.imm << "]";
            break;
        case INST_T16_LDRH_IMM:
            oss << "ldrh\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", #" << fields.imm << "]";
            break;
        case INST_T16_STR_SP:
            oss << "str\t" << reg_name(fields.rd) << ", [sp, #" << fields.imm << "]";
            break;
        case INST_T16_LDR_SP:
            oss << "ldr\t" << reg_name(fields.rd) << ", [sp, #" << fields.imm << "]";
            break;

        // T16 Address Generation
        case INST_T16_ADD_PC:
            oss << "add\t" << reg_name(fields.rd) << ", pc, #" << fields.imm;
            break;
        case INST_T16_ADD_SP:
            oss << "add\t" << reg_name(fields.rd) << ", sp, #" << fields.imm;
            break;
        case INST_T16_ADD_SP_IMM7:
            oss << "add\tsp, #" << fields.imm;
            break;
        case INST_T16_SUB_SP_IMM7:
            oss << "sub\tsp, #" << fields.imm;
            break;

        // T16 Stack Operations
        case INST_T16_PUSH:
            oss << "push\t" << format_reg_list(fields.reg_list);
            break;
        case INST_T16_POP:
            oss << "pop\t" << format_reg_list(fields.reg_list);
            break;

        // T16 System Instructions  
        case INST_T16_CPS:
            if (fields.alu_op == 1) {
                oss << "cpsid\t";
            } else {
                oss << "cpsie\t";
            }
            if (fields.imm & 0x1) oss << "i";
            if (fields.imm & 0x2) oss << "f";
            if (fields.imm & 0x4) oss << "a";
            break;

        // T16 Multiple Load/Store
        case INST_T16_STMIA:
            oss << "stmia\t" << reg_name(fields.rn) << "!, " << format_reg_list(fields.reg_list);
            break;
        case INST_T16_LDMIA:
            oss << "ldmia\t" << reg_name(fields.rn) << "!, "  << format_reg_list(fields.reg_list);
            break;

        // T16 Branch Instructions  
        case INST_T16_B_COND: {
            oss << "b" << cond_suffix(fields.cond) << "\t#" << (int32_t)fields.imm;
            break;
        }
        case INST_T16_B:
            oss << "b\t#" << (int32_t)fields.imm;
            break;
        case INST_T16_SVC:
            oss << "svc\t#" << fields.imm;
            break;

        // T16 Hints and System
        case INST_T16_NOP:
            oss << "nop";
            break;
        case INST_T16_WFI:
            oss << "wfi";
            break;
        case INST_T16_WFE:
            oss << "wfe";
            break;
        case INST_T16_SEV:
            oss << "sev";
            break;
        case INST_T16_YIELD:
            oss << "yield";
            break;
        case INST_T16_BKPT:
            oss << "bkpt\t#0x" << std::hex << std::setfill('0') << std::setw(4) << fields.imm;
            break;
    
        // T32 Branch and Table Branch
        case INST_T32_B:
            oss << "b.w\t#" << (int32_t)fields.imm;
            break;
        case INST_T32_B_COND:
            oss << "b" << cond_suffix(fields.cond) << ".w\t#" << (int32_t)fields.imm;
            break;
        case INST_T32_BL:
            oss << "bl\t#" << (int32_t)fields.imm;
            break;

        case INST_T32_DSB:
            oss << "dsb";
            break;
        case INST_T32_DMB:
            oss << "dmb";
            break;
        case INST_T32_ISB:
            oss << "isb";
            break;

#if HAS_SYSTEM_REGISTERS
        // T32 System Register Access
        case INST_T32_MRS: {
            oss << "mrs\t" << reg_name(fields.rd) << ", ";
            switch (fields.imm) {
                case 0: oss << "APSR"; break;
                case 1: oss << "IAPSR"; break;
                case 2: oss << "EAPSR"; break;
                case 3: oss << "XPSR"; break;
                case 5: oss << "IPSR"; break;
                case 6: oss << "EPSR"; break;
                case 7: oss << "IEPSR"; break;
                case 8: oss << "MSP"; break;
                case 9: oss << "PSP"; break;
                case 16: oss << "PRIMASK"; break;
                case 17: oss << "BASEPRI"; break;
                case 18: oss << "BASEPRI_MAX"; break;
                case 19: oss << "FAULTMASK"; break;
                case 20: oss << "CONTROL"; break;
                default: oss << "SYS_" << fields.imm; break;
            }
            break;
        }
        case INST_T32_MSR: {
            oss << "msr\t";
            switch (fields.imm) {
                case 0: oss << "APSR"; break;
                case 1: oss << "IAPSR"; break;
                case 2: oss << "EAPSR"; break;
                case 3: oss << "XPSR"; break;
                case 5: oss << "IPSR"; break;
                case 6: oss << "EPSR"; break;
                case 7: oss << "IEPSR"; break;
                case 8: oss << "MSP"; break;
                case 9: oss << "PSP"; break;
                case 16: oss << "PRIMASK"; break;
                case 17: oss << "BASEPRI"; break;
                case 18: oss << "BASEPRI_MAX"; break;
                case 19: oss << "FAULTMASK"; break;
                case 20: oss << "CONTROL"; break;
                default: oss << "SYS_" << fields.imm; break;
            }
            oss << ", " << reg_name(fields.rn);
            break;
        }
#endif
#if SUPPORTS_ARMV7_M
        // T16 ARMv7-M Extensions
        case INST_T16_CBZ:
            oss << "cbz\t" << reg_name(fields.rn) << ", #" << fields.imm;
            break;
        case INST_T16_CBNZ:
            oss << "cbnz\t" << reg_name(fields.rn) << ", #" << fields.imm;
            break;

        // T32 Data Processing Instructions
        case INST_T32_MOV_IMM:
            oss << "mov.w" << cond_suffix(fields.cond) << "\t" << reg_name(fields.rd) << ", #" << fields.imm;
            break;
        case INST_T32_MOVS_IMM:
            oss << "movs.w" << cond_suffix(fields.cond) << "\t" << reg_name(fields.rd) << ", #" << fields.imm;
            break;
        case INST_T32_MVN_IMM:
            oss << "mvn.w" << cond_suffix(fields.cond) << "\t" << reg_name(fields.rd) << ", #" << fields.imm;
            break;
        case INST_T32_ADD_IMM:
            oss << "add.w" << cond_suffix(fields.cond) << "\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", #" << fields.imm;
            break;
        case INST_T32_SUB_IMM:
            oss << "sub.w" << cond_suffix(fields.cond) << "\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", #" << fields.imm;
            break;
        case INST_T32_ADDW:
            oss << "addw\t" << reg_name(fields.rd) << ", " << (fields.rn == 15 ? "pc" : "" + reg_name(fields.rn)) << ", #" << fields.imm;
            break;
        case INST_T32_SUBW:
            oss << "subw\t" << reg_name(fields.rd) << ", " << (fields.rn == 15 ? "pc" : "" + reg_name(fields.rn)) << ", #" << fields.imm;
            break;
        case INST_T32_MOVW:
            oss << "movw\t" << reg_name(fields.rd) << ", #" << fields.imm;
            break;
        case INST_T32_MOVT:
            oss << "movt\t" << reg_name(fields.rd) << ", #" << fields.imm;
            break;
        case INST_T32_ADC_IMM:
            oss << "adc.w" << cond_suffix(fields.cond) << "\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", #" << fields.imm;
            break;
        case INST_T32_SBC_IMM:
            oss << "sbc.w" << cond_suffix(fields.cond) << "\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", #" << fields.imm;
            break;
        case INST_T32_RSB_IMM:
            oss << "rsb.w\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", #" << fields.imm;
            break;
        case INST_T32_AND_IMM:
            oss << "and.w\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", #" << fields.imm;
            break;
        case INST_T32_ORR_IMM:
            oss << "orr.w\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", #" << fields.imm;
            break;
        case INST_T32_EOR_IMM:
            oss << "eor.w\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", #" << fields.imm;
            break;
        case INST_T32_BIC_IMM:
            oss << "bic.w\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", #" << fields.imm;
            break;
        case INST_T32_ORN_IMM:
            oss << "orn.w\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", #" << fields.imm;
            break;
        case INST_T32_CMP_IMM:
            oss << "cmp.w\t" << reg_name(fields.rn) << ", #" << fields.imm;
            break;
        case INST_T32_CMN_IMM:
            oss << "cmn.w\t" << reg_name(fields.rn) << ", #" << fields.imm;
            break;
        case INST_T32_TST_IMM:
            oss << "tst.w\t" << reg_name(fields.rn) << ", #" << fields.imm;
            break;
        case INST_T32_TEQ_IMM:
            oss << "teq.w\t" << reg_name(fields.rn) << ", #" << fields.imm;
            break;

        // T32 Data Processing Instructions (Register operands)
        case INST_T32_AND_REG:
            oss << format_t32_data_proc_reg("and", false, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_ANDS_REG:
            oss << format_t32_data_proc_reg("ands", true, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_ORR_REG:
            oss << format_t32_data_proc_reg("orr", false, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_ORRS_REG:
            oss << format_t32_data_proc_reg("orrs", true, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_ORN_REG:
            oss << format_t32_data_proc_reg("orn", false, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_ORNS_REG:
            oss << format_t32_data_proc_reg("orns", true, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_EOR_REG:
            oss << format_t32_data_proc_reg("eor", false, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_MOV_REG:
        case INST_T32_MOVS_REG:
            oss << (fields.type == INST_T32_MOVS_REG ? "movs.w" : "mov.w")
                << cond_suffix(fields.cond) << "\t" << reg_name(fields.rd)
                << ", " << reg_name(fields.rm);
            if (fields.shift_amount > 0) {
                const char* shift_names[] = {"lsl", "lsr", "asr", "ror"};
                oss << ", " << shift_names[fields.shift_type & 0x3] << " #" << std::to_string(fields.shift_amount);
            }
            break;
        case INST_T32_MVN_REG:
        case INST_T32_MVNS_REG:
            oss << (fields.type == INST_T32_MVNS_REG ? "mvns.w" : "mvn.w")
                << cond_suffix(fields.cond) << "\t" << reg_name(fields.rd)
                << ", " << reg_name(fields.rm);
            //append_shift_suffix(oss, fields);
            break;
        case INST_T32_EORS_REG:
            oss << format_t32_data_proc_reg("eor", true, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_BIC_REG:
            oss << format_t32_data_proc_reg("bic", false, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_BICS_REG:
            oss << format_t32_data_proc_reg("bic", true, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_ADD_REG:
            oss << format_t32_data_proc_reg("add", false, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_ADDS_REG:
            oss << format_t32_data_proc_reg("add", true, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_SUB_REG:
            oss << format_t32_data_proc_reg("sub", false, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_SUBS_REG:
            oss << format_t32_data_proc_reg("sub", true, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_ADC_REG:
            oss << format_t32_data_proc_reg("adc", false, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_ADCS_REG:
            oss << format_t32_data_proc_reg("adcs", true, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_SBC_REG:
            oss << format_t32_data_proc_reg("sbc", false, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_SBCS_REG:
            oss << format_t32_data_proc_reg("sbcs", true, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_RSB_REG:
            oss << format_t32_data_proc_reg("rsb", false, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_RSBS_REG:
            oss << format_t32_data_proc_reg("rsbs", true, fields.rd, fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_TST_REG:
            oss << format_t32_compare_reg("tst", fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_TEQ_REG:
            oss << format_t32_compare_reg("teq", fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_CMP_REG:
            oss << format_t32_compare_reg("cmp", fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;
        case INST_T32_CMN_REG:
            oss << format_t32_compare_reg("cmn", fields.rn, fields.rm, fields.shift_type, fields.shift_amount);
            break;

        // T32 Shift Instructions (register)
        case INST_T32_LSL_REG:
        case INST_T32_LSLS_REG:
            oss << (fields.type == INST_T32_LSLS_REG ? "lsls.w" : "lsl.w")
                << "\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm) << ", " << reg_name(fields.rn);
            break;
        case INST_T32_LSR_REG:
        case INST_T32_LSRS_REG:
            oss << (fields.type == INST_T32_LSRS_REG ? "lsrs.w" : "lsr.w")
                << "\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm) << ", " << reg_name(fields.rn);
            break;
        case INST_T32_ASR_REG:
        case INST_T32_ASRS_REG:
            oss << (fields.type == INST_T32_ASRS_REG ? "asrs.w" : "asr.w")
                << "\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm) << ", " << reg_name(fields.rn);
            break;
        case INST_T32_ROR_REG:
        case INST_T32_RORS_REG:
            oss << (fields.type == INST_T32_RORS_REG ? "rors.w" : "ror.w")
                << "\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm) << ", " << reg_name(fields.rn);
            break;

        // T32 Load/Store Instructions
        case INST_T32_LDR_IMM:
            oss << "ldr.w\t" << reg_name(fields.rd) << ", " << format_address(fields.rn, fields.imm, true, false, fields.negative_offset);
            break;
        case INST_T32_STR_IMM:
            oss << "str.w\t" << reg_name(fields.rd) << ", " << format_address(fields.rn, fields.imm, true, false, fields.negative_offset);
            break;
        case INST_T32_LDRB_IMM:
            oss << "ldrb.w\t" << reg_name(fields.rd) << ", " << format_address(fields.rn, fields.imm, true, false, fields.negative_offset);
            break;
        case INST_T32_STRB_IMM:
            oss << "strb.w\t" << reg_name(fields.rd) << ", " << format_address(fields.rn, fields.imm, true, false, fields.negative_offset);
            break;
        case INST_T32_LDRH_IMM:
            oss << "ldrh.w\t" << reg_name(fields.rd) << ", " << format_address(fields.rn, fields.imm, true, false, false);
            break;
        case INST_T32_STRH_IMM:
            oss << "strh.w\t" << reg_name(fields.rd) << ", " << format_address(fields.rn, fields.imm, true, false, false);
            break;
        case INST_T32_STR_PRE_POST:
            oss << "str.w\t" << reg_name(fields.rd) << ", " << format_address(fields.rn, fields.imm, fields.pre_indexed, fields.writeback, fields.negative_offset);
            break;
        case INST_T32_STRB_PRE_POST:
            oss << "strb.w\t" << reg_name(fields.rd) << ", " << format_address(fields.rn, fields.imm, fields.pre_indexed, fields.writeback, fields.negative_offset);
            break;
        case INST_T32_STRH_PRE_POST:
            oss << "strh.w\t" << reg_name(fields.rd) << ", " << format_address(fields.rn, fields.imm, fields.pre_indexed, fields.writeback, fields.negative_offset);
            break;
        case INST_T32_LDR_PRE_POST:
            oss << "ldr.w\t" << reg_name(fields.rd) << ", " << format_address(fields.rn, fields.imm, fields.pre_indexed, fields.writeback, fields.negative_offset);
            break;
        case INST_T32_LDRB_PRE_POST:
            oss << "ldrb.w\t" << reg_name(fields.rd) << ", " << format_address(fields.rn, fields.imm, fields.pre_indexed, fields.writeback, fields.negative_offset);
            break;
        case INST_T32_LDRH_PRE_POST:
            oss << "ldrh.w\t" << reg_name(fields.rd) << ", " << format_address(fields.rn, fields.imm, fields.pre_indexed, fields.writeback, fields.negative_offset);
            break;
        case INST_T32_LDRSB_IMM:
            oss << "ldrsb.w\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", #" << fields.imm << "]";
            break;
        case INST_T32_LDRSH_IMM:
            oss << "ldrsh.w\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", #" << fields.imm << "]";
            break;
        case INST_T32_LDR_LIT:
            oss << "ldr.w\t" << reg_name(fields.rd) << ", [pc, #" << (int32_t)fields.imm << "]";
            break;
        case INST_T32_LDR_REG:
            oss << "ldr.w\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            if (fields.shift_amount > 0) {
                oss << ", lsl #" << (int)fields.shift_amount;
            }
            oss << "]";
            break;
        case INST_T32_LDRT:
            oss << "ldrt\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", #" << fields.imm << "]";
            break;
        case INST_T32_LDRB_REG:
            oss << "ldrb.w\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            if (fields.shift_amount > 0) {
                oss << ", lsl #" << (int)fields.shift_amount;
            }
            oss << "]";
            break;
        case INST_T32_LDRH_REG:
            oss << "ldrh.w\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            if (fields.shift_amount > 0) {
                oss << ", lsl #" << (int)fields.shift_amount;
            }
            oss << "]";
            break;
        case INST_T32_LDRSB_REG:
            oss << "ldrsb.w\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            if (fields.shift_amount > 0) {
                oss << ", lsl #" << (int)fields.shift_amount;
            }
            oss << "]";
            break;
        case INST_T32_LDRSH_REG:
            oss << "ldrsh.w\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            if (fields.shift_amount > 0) {
                oss << ", lsl #" << (int)fields.shift_amount;
            }
            oss << "]";
            break;
        case INST_T32_STR_REG:
            oss << "str.w\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            if (fields.shift_amount > 0) {
                oss << ", lsl #" << (int)fields.shift_amount;
            }
            oss << "]";
            break;
        case INST_T32_STRB_REG:
            oss << "strb.w\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            if (fields.shift_amount > 0) {
                oss << ", lsl #" << (int)fields.shift_amount;
            }
            oss << "]";
            break;
        case INST_T32_STRH_REG:
            oss << "strh.w\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            if (fields.shift_amount > 0) {
                oss << ", lsl #" << (int)fields.shift_amount;
            }
            oss << "]";
            break;
        case INST_T32_LDRD:
            oss << "ldrd\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm) << ", [";
            if (fields.rn == 15) oss << "pc";
            else oss << reg_name(fields.rn);
            oss << ", #" << fields.imm << "]";
            break;
        case INST_T32_STRD:
            oss << "strd\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm) << ", [";
            if (fields.rn == 15) oss << "pc";
            else oss << reg_name(fields.rn);
            oss << ", #" << fields.imm << "]";
            break;
        // T32 Bit Manipulation
        case INST_T32_CLZ:
            oss << "clz\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T32_RBIT:
            oss << "rbit\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T32_REV:
            oss << "rev\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T32_REV16:
            oss << "rev16\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;
        case INST_T32_REVSH:
            oss << "revsh\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm);
            break;


        case INST_T32_TBB:
            oss << "tbb\t[" << reg_name(fields.rn) << ", " << reg_name(fields.rm) << "]";
            break;
        case INST_T32_TBH:
            oss << "tbh\t[" << reg_name(fields.rn) << ", " << reg_name(fields.rm) << ", lsl #1]";
            break;

        // T32 Exclusive Access Instructions
#if HAS_EXCLUSIVE_ACCESS
        case INST_T32_LDREX:
            oss << "ldrex\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << "]";
            break;
        case INST_T32_LDREXB:
            oss << "ldrexb\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << "]";
            break;
        case INST_T32_LDREXH:
            oss << "ldrexh\t" << reg_name(fields.rd) << ", [" << reg_name(fields.rn) << "]";
            break;
        case INST_T32_STREX:
            oss << "strex\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm) << ", [" << reg_name(fields.rn) << "]";
            break;
        case INST_T32_STREXB:
            oss << "strexb\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm) << ", [" << reg_name(fields.rn) << "]";
            break;
        case INST_T32_STREXH:
            oss << "strexh\t" << reg_name(fields.rd) << ", " << reg_name(fields.rm) << ", [" << reg_name(fields.rn) << "]";
            break;
#endif

        // T32 Hardware Divide
#if HAS_HARDWARE_DIVIDE
        case INST_T32_UDIV:
            oss << "udiv\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            break;
        case INST_T32_SDIV:
            oss << "sdiv\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            break;
        case INST_T32_MUL:
            oss << "mul.w\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            break;
        case INST_T32_MLA:
            oss << "mla\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", " << reg_name(fields.rm) << ", " << reg_name(fields.rs);
            break;
        case INST_T32_MLS:
            oss << "mls\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", " << reg_name(fields.rm) << ", " << reg_name(fields.rs);
            break;
        case INST_T32_UMULL:
            oss << "umull\t" << reg_name(fields.rd) << ", " << (int)fields.rs << ", " << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            break;
        case INST_T32_SMULL:
            oss << "smull\t" << reg_name(fields.rd) << ", " << (int)fields.rs << ", " << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            break;
        case INST_T32_UMLAL:
            oss << "umlal\t" << reg_name(fields.rd) << ", " << (int)fields.rs << ", " << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            break;
        case INST_T32_SMLAL:
            oss << "smlal\t" << reg_name(fields.rd) << ", " << (int)fields.rs << ", " << reg_name(fields.rn) << ", " << reg_name(fields.rm);
            break;
#endif

#if HAS_SATURATING_ARITHMETIC
        // T32 Saturating Arithmetic
        case INST_T32_SSAT: {
            // SSAT Rd, #imm, Rm [, shift]
            uint32_t sat_imm = (fields.imm >> 16) & 0x1F;   // Saturation limit
            uint32_t shift_type = (fields.imm >> 8) & 0x1;  // Shift type (0=LSL, 1=ASR)
            uint32_t shift_amount = fields.imm & 0x7F;      // Shift amount
            
            oss << "ssat\t" << reg_name(fields.rd) << ", #" << sat_imm << ", " << reg_name(fields.rn);
            if (shift_amount > 0) {
                oss << ", " << (shift_type ? "asr" : "lsl") << " #" << shift_amount;
            }
            break;
        }
        case INST_T32_USAT: {
            // USAT Rd, #imm, Rm [, shift]
            uint32_t sat_imm = (fields.imm >> 16) & 0x1F;   // Saturation limit
            uint32_t shift_type = (fields.imm >> 8) & 0x1;  // Shift type (0=LSL, 1=ASR)
            uint32_t shift_amount = fields.imm & 0x7F;      // Shift amount
            
            oss << "usat\t" << reg_name(fields.rd) << ", #" << sat_imm << ", " << reg_name(fields.rn);
            if (shift_amount > 0) {
                oss << ", " << (shift_type ? "asr" : "lsl") << " #" << shift_amount;
            }
            break;
        }
#endif

        // T32 System Instructions
        case INST_T32_NOP:
            oss << "nop.w";
            break;
        case INST_T32_CLREX:
            oss << "clrex";
            break;
#endif

        // T32 Multiple Load/Store Instructions
#if SUPPORTS_ARMV7_M
        case INST_T32_LDMIA:
            if (fields.rn == 13) { // SP
                oss << "ldmia.w\tsp!, " << format_reg_list(fields.reg_list);
            } else {
                oss << "ldmia.w\t" << reg_name(fields.rn) << "!, " << format_reg_list(fields.reg_list);
            }
            break;
        case INST_T32_LDMDB:
            if (fields.rn == 13) { // SP  
                oss << "ldmia.w\tsp!, " << format_reg_list(fields.reg_list); // Show as LDMIA for SP
            } else {
                oss << "ldmdb.w\t" << reg_name(fields.rn) << "!, " << format_reg_list(fields.reg_list);
            }
            break;
        case INST_T32_STMIA:
            oss << "stmia.w\t" << reg_name(fields.rn) << "!, " << format_reg_list(fields.reg_list);
            break;
        case INST_T32_STMDB:
            if (fields.rn == 13) { // SP
                oss << "stmdb\tsp!, " << format_reg_list(fields.reg_list);
            } else {
                oss << "stmdb.w\t" << reg_name(fields.rn) << "!, " << format_reg_list(fields.reg_list);
            }
            break;
#endif

#if HAS_IT_BLOCKS
        // IT (If-Then) block instruction
        case INST_T16_IT: {
            oss << "it";
            uint32_t mask = fields.imm & 0xF;
            uint32_t firstcond = fields.cond & 0xF;

            auto it_len_from_mask = [](uint32_t m) -> int {
                // Find the length by locating the trailing 1 bit
                // The mask has a trailing 1 bit that determines the length
                if (m == 0) return 0;
                if (m & 0x1) return 4;  // xxx1 -> length 4
                if (m & 0x2) return 3;  // xx10 -> length 3  
                if (m & 0x4) return 2;  // x100 -> length 2
                if (m & 0x8) return 1;  // 1000 -> length 1
                return 0;
            };
            int len = it_len_from_mask(mask);
            
            // Generate the T/E sequence based on mask bits
            // For each position, if mask bit is 1: same condition (T), if 0: inverted condition (E)
            for (int i = 1; i < len; ++i) {
                int bitpos = 4 - i; // Bit positions: 3,2,1 for positions 1,2,3
                char ch = ((mask >> bitpos) & 0x1) == (firstcond & 0x1) ? 't' : 'e';
                oss << ch;
            }

            oss << "\t" << cond_suffix(firstcond);
            break;
        }
#endif

#if HAS_BITFIELD_INSTRUCTIONS
        // T32 Bitfield Instructions
        case INST_T32_BFI: {
            uint32_t width = fields.imm & 0xFF;        // bits [7:0]
            uint32_t lsb = (fields.imm >> 8) & 0xFF; // bits [15:8]  
            oss << "bfi\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", #" << lsb << ", #" << width;
            break;
        }
        case INST_T32_BFC: {
            uint32_t width = fields.imm & 0xFF;        // bits [7:0]
            uint32_t lsb = (fields.imm >> 8) & 0xFF; // bits [15:8]
            oss << "bfc\t" << reg_name(fields.rd) << ", #" << lsb << ", #" << width;
            break;
        }
        case INST_T32_UBFX: {
            uint32_t width = fields.imm & 0xFF;         // bits [7:0]
            uint32_t lsb = (fields.imm >> 8) & 0xFF; // bits [15:8]
            oss << "ubfx\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", #" << lsb << ", #" << width;
            break;
        }
        case INST_T32_SBFX: {
            uint32_t width = fields.imm & 0xFF;         // bits [7:0]
            uint32_t lsb = (fields.imm >> 8) & 0xFF; // bits [15:8]
            oss << "sbfx\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn) << ", #" << lsb << ", #" << width;
            break;
        }
        
        // Sign/Zero Extend Instructions
        case INST_T32_SXTH:
            oss << "sxth.w\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn);
            break;
        case INST_T32_SXTB:
            oss << "sxtb.w\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn);
            break;
        case INST_T32_UXTH:
            oss << "uxth.w\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn);
            break;
        case INST_T32_UXTB:
            oss << "uxtb.w\t" << reg_name(fields.rd) << ", " << reg_name(fields.rn);
            break;
#endif

        default:
            oss << "unknown\t(type=" << fields.type << ")";
            break;
    }
    
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
#if HAS_IT_BLOCKS
    // Check IT block state for conditional execution (ARMv7-M)
    bool should_execute = true;
    if (m_registers->in_it_block() && fields.type != INST_T16_IT) {
        // Determine if this instruction should execute based on IT condition & T/E slot
        // NOTE: Condition is evaluated once when IT block starts, not per instruction
        uint8_t it_condition = m_registers->get_it_firstcond();
        bool then_slot = m_registers->current_it_then();
        
        // Get the condition result that was saved when IT block started
        bool cond_ok = m_registers->get_it_condition_result();
        should_execute = then_slot ? cond_ok : !cond_ok;
        {
            std::string dbg = std::string("IT: cond=") + std::to_string(it_condition) +
                              ", then=" + (then_slot ? "T" : "E") +
                              ", cond_ok=" + (cond_ok ? "1" : "0") +
                              ", exec=" + (should_execute ? "1" : "0");
            LOG_DEBUG(dbg);
        }

        // Advance IT state after checking
        m_registers->advance_it_state();

        if (!m_registers->in_it_block()) {
            LOG_DEBUG("IT: Block completed");
        }
    }

    // Skip execution if IT condition not met (except for IT instruction itself)
    if (!should_execute) {
        LOG_DEBUG(std::string("SKIP: ") + format_instruction(fields) + " (IT)");
        return false; // PC will be advanced by caller
    }
#endif
    std::stringstream ss;
    if (fields.is_32bit) {
        // For 32-bit instructions, print as two 16-bit halfwords
        uint16_t first_half = (fields.opcode >> 16) & 0xFFFF;
        uint16_t second_half = fields.opcode & 0xFFFF;
        ss << std::hex << m_registers->get_pc()  << ":\t"<< std::hex << first_half << " " << std::hex << std::setw(4) << std::setfill('0') << second_half << "\t";
    } else {
        // For 16-bit instructions, print as single halfword
        ss << std::hex << m_registers->get_pc()  << ":\t\t "<< std::hex << std::setw(4) << std::setfill('0') << (fields.opcode & 0xFFFF) << "\t";
    }

    // Log only when we're going to execute (or if IT not built)
    LOG_DEBUG(ss.str() + format_instruction(fields));

    bool pc_changed = false;
    
    switch (fields.type) {
        // Branches
        case INST_T16_B_COND:
        case INST_T16_B:
        case INST_T16_BX:
#if HAS_BLX_REGISTER
        case INST_T16_BLX:
#endif
#if HAS_T32_BL
        case INST_T32_B:
        case INST_T32_B_COND:
        case INST_T32_BL:
#endif
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
        case INST_T16_ADD_SP_IMM7:
        case INST_T16_SUB_SP_IMM7:
        case INST_T16_ADD_PC:
        case INST_T16_ADD_SP:
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
        // Extend instructions
        case INST_T16_EXTEND:
            pc_changed = execute_extend(fields);
            break;
        // Reverse instructions
        case INST_T16_REV:
        case INST_T16_REV16:
        case INST_T16_REVSH:
            pc_changed = execute_rev(fields);
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
        case INST_T16_BKPT:
            pc_changed = execute_miscellaneous(fields);
            break;
        case INST_T16_SVC:
            pc_changed = execute_exception(fields);
            break;
#if HAS_MEMORY_BARRIERS
        // T32 Memory barriers
        case INST_T32_DSB:
        case INST_T32_DMB:
        case INST_T32_ISB:
            pc_changed = execute_memory_barrier(fields);
            break;
#endif
#if HAS_SYSTEM_REGISTERS
        // T32 System register access
        case INST_T32_MSR:
            pc_changed = execute_msr(fields);
            break;
        case INST_T32_MRS:
            pc_changed = execute_mrs(fields);
            break;
#endif
#if HAS_CBZ_CBNZ
        // ARMv7-M Compare and Branch
        case INST_T16_CBZ:
        case INST_T16_CBNZ:
            pc_changed = execute_cbz_cbnz(fields);
            break;
#endif
#if HAS_IT_BLOCKS
        // ARMv7-M If-Then blocks
        case INST_T16_IT:
            pc_changed = execute_it(fields);
            break;
#endif
#if HAS_EXTENDED_HINTS
        // ARMv7-M Extended Hints
        case INST_T16_NOP:
        case INST_T16_WFI:
        case INST_T16_WFE:
        case INST_T16_SEV:
        case INST_T16_YIELD:
            pc_changed = execute_extended_hint(fields);
            break;
#endif

#if SUPPORTS_ARMV7_M
        // ARMv7-M T32 Instructions
        case INST_T32_TBB:
        case INST_T32_TBH:
            pc_changed = execute_table_branch(fields, data_bus);
            break;
        case INST_T32_NOP:
            // NOP.W - No operation, just continue
            LOG_DEBUG("NOP.W");
            break;
        case INST_T32_CLREX:
            pc_changed = execute_clrex(fields);
            break;
        // T32 Data Processing Instructions
        case INST_T32_MOV_IMM:
        case INST_T32_MOVS_IMM:
        case INST_T32_MVN_IMM:
        case INST_T32_ADD_IMM:
        case INST_T32_SUB_IMM:
        case INST_T32_ADDW:
        case INST_T32_SUBW:
        case INST_T32_MOVW:
        case INST_T32_MOVT:
        case INST_T32_ADR:
        case INST_T32_ADC_IMM:
        case INST_T32_SBC_IMM:
        case INST_T32_RSB_IMM:
        case INST_T32_AND_IMM:
        case INST_T32_ORR_IMM:
        case INST_T32_EOR_IMM:
        case INST_T32_BIC_IMM:
        case INST_T32_ORN_IMM:
        case INST_T32_CMP_IMM:
        case INST_T32_CMN_IMM:
        case INST_T32_TST_IMM:
        case INST_T32_TEQ_IMM:
            pc_changed = execute_t32_data_processing(fields);
            break;
        // T32 Data Processing Instructions (Register operands)
        case INST_T32_AND_REG:
        case INST_T32_ANDS_REG:
        case INST_T32_ORR_REG:
        case INST_T32_ORRS_REG:
        case INST_T32_ORN_REG:
        case INST_T32_ORNS_REG:
        case INST_T32_EOR_REG:
        case INST_T32_EORS_REG:
        case INST_T32_BIC_REG:
        case INST_T32_BICS_REG:
        case INST_T32_ADD_REG:
        case INST_T32_ADDS_REG:
        case INST_T32_SUB_REG:
        case INST_T32_SUBS_REG:
        case INST_T32_ADC_REG:
        case INST_T32_ADCS_REG:
        case INST_T32_SBC_REG:
        case INST_T32_SBCS_REG:
        case INST_T32_RSB_REG:
        case INST_T32_RSBS_REG:
        case INST_T32_MOV_REG:
        case INST_T32_MOVS_REG:
        case INST_T32_MVN_REG:
        case INST_T32_MVNS_REG:
        case INST_T32_PKH_REG:
        case INST_T32_TST_REG:
        case INST_T32_TEQ_REG:
        case INST_T32_CMP_REG:
        case INST_T32_CMN_REG:
#if HAS_DSP_EXTENSIONS
        case INST_T32_SADD16: case INST_T32_SSUB16: case INST_T32_SASX: case INST_T32_SSAX:
        case INST_T32_SADD8: case INST_T32_SSUB8:
        case INST_T32_QADD16: case INST_T32_QSUB16: case INST_T32_QASX: case INST_T32_QSAX: case INST_T32_QADD8: case INST_T32_QSUB8:
        case INST_T32_UADD16: case INST_T32_USUB16: case INST_T32_UASX: case INST_T32_USAX: case INST_T32_UADD8: case INST_T32_USUB8:
        case INST_T32_UQADD16: case INST_T32_UQSUB16: case INST_T32_UQASX: case INST_T32_UQSAX: case INST_T32_UQADD8: case INST_T32_UQSUB8:
        case INST_T32_SHADD16: case INST_T32_SHSUB16: case INST_T32_SHASX: case INST_T32_SHSAX: case INST_T32_SHADD8: case INST_T32_SHSUB8:
        case INST_T32_UHADD16: case INST_T32_UHSUB16: case INST_T32_UHASX: case INST_T32_UHSAX: case INST_T32_UHADD8: case INST_T32_UHSUB8:
            execute_parallel_addsub(m_registers, fields);
            pc_changed = false; // does not modify pc
            break;
#endif
            pc_changed = execute_t32_data_processing(fields);
            break;
        // T32 Shift Instructions (register)
        case INST_T32_LSL_REG:
        case INST_T32_LSLS_REG:
        case INST_T32_LSR_REG:
        case INST_T32_LSRS_REG:
        case INST_T32_ASR_REG:
        case INST_T32_ASRS_REG:
        case INST_T32_ROR_REG:
        case INST_T32_RORS_REG:
            pc_changed = execute_t32_shift_register(fields);
            break;
        // T32 Load/Store Instructions
        case INST_T32_LDR_IMM:
        case INST_T32_LDRB_IMM:
        case INST_T32_LDRH_IMM:
        case INST_T32_LDRSB_IMM:
        case INST_T32_LDRSH_IMM:
        case INST_T32_STR_IMM:
        case INST_T32_STRB_IMM:
        case INST_T32_STRH_IMM:
        case INST_T32_STR_PRE_POST:
        case INST_T32_STRB_PRE_POST:
        case INST_T32_STRH_PRE_POST:
        case INST_T32_LDR_PRE_POST:
        case INST_T32_LDRB_PRE_POST:
        case INST_T32_LDRH_PRE_POST:
        case INST_T32_LDR_LIT:
        case INST_T32_LDR_REG:
        case INST_T32_LDRB_REG:
        case INST_T32_LDRH_REG:
        case INST_T32_LDRSB_REG:
        case INST_T32_LDRSH_REG:
        case INST_T32_LDRT:
        case INST_T32_STR_REG:
        case INST_T32_STRB_REG:
        case INST_T32_STRH_REG:
            pc_changed = execute_t32_load_store(fields, data_bus);
            break;
        case INST_T32_LDRD:
        case INST_T32_STRD:
            pc_changed = execute_t32_dual_load_store(fields, data_bus);
            break;
        // T32 Multiple Load/Store Instructions
        case INST_T32_LDMIA:
        case INST_T32_LDMDB:
        case INST_T32_STMIA:
        case INST_T32_STMDB:
            pc_changed = execute_t32_multiple_load_store(fields, data_bus);
            break;
#if HAS_EXCLUSIVE_ACCESS
        case INST_T32_LDREX:
        case INST_T32_LDREXB:
        case INST_T32_LDREXH:
            pc_changed = execute_exclusive_load(fields, data_bus);
            break;
        case INST_T32_STREX:
        case INST_T32_STREXB:
        case INST_T32_STREXH:
            pc_changed = execute_exclusive_store(fields, data_bus);
            break;
#endif
#if HAS_HARDWARE_DIVIDE
        case INST_T32_UDIV:
        case INST_T32_SDIV:
            pc_changed = execute_divide(fields);
            break;
        case INST_T32_MUL:
            pc_changed = execute_mul(fields);
            break;
        case INST_T32_MLA:
            pc_changed = execute_mla(fields);
            break;
        case INST_T32_MLS:
            pc_changed = execute_mls(fields);
            break;
        case INST_T32_UMULL:
        case INST_T32_SMULL:
        case INST_T32_UMLAL:
        case INST_T32_SMLAL:
            pc_changed = execute_long_multiply(fields);
            break;
#endif
#if HAS_BITFIELD_INSTRUCTIONS
        case INST_T32_BFI:
        case INST_T32_BFC:
        case INST_T32_UBFX:
        case INST_T32_SBFX:
            pc_changed = execute_bitfield(fields);
            break;
            
        // Sign/Zero Extend Instructions
        case INST_T32_SXTH:
        case INST_T32_SXTB:
        case INST_T32_UXTH:
        case INST_T32_UXTB:
            pc_changed = execute_extend(fields);
            break;
#endif
#if HAS_SATURATING_ARITHMETIC
        case INST_T32_SSAT:
        case INST_T32_USAT:
            pc_changed = execute_saturate(fields);
            break;
#endif
#if HAS_BIT_MANIPULATION
        case INST_T32_CLZ:
        case INST_T32_RBIT:
        case INST_T32_REV:
        case INST_T32_REV16:
        case INST_T32_REVSH:
            pc_changed = execute_bit_manipulation(fields);
            break;
#endif
#endif // SUPPORTS_ARMV7_M

        default:
            LOG_WARNING("Unknown instruction type: " + std::to_string(fields.type) + " (may not be supported in " ARM_CORE_NAME ")");
            break;
    }
    
    return pc_changed;
}

bool Execute::execute_branch(const InstructionFields& fields)
{
    // For conditional branches, check condition
    if (fields.type == INST_T16_B_COND || fields.type == INST_T32_B_COND) {
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
#if HAS_BLX_REGISTER
            // BLX - save return address in LR
            m_registers->write_register(14, current_pc + 2 + 1); // +1 for Thumb bit
#else
            LOG_WARNING("BLX instruction not supported in " ARM_CORE_NAME ", treating as BX");
#endif
        }
        
        // BX to EXC_RETURN magic value should perform an exception return
        if (m_cpu && m_cpu->try_exception_return(new_pc)) {
            LOG_DEBUG("Exception return via BX R" + reg_name(fields.rm));
            // Clear any active IT state on control-flow change
            m_registers->clear_it_state();
            return true; // PC updated by exception return
        }

        // BX/BLX can switch between ARM and Thumb modes
        // Bit 0 indicates Thumb mode (which we always use)
        m_registers->set_pc(new_pc & ~1);
        // Clear any active IT state on control-flow change
        m_registers->clear_it_state();
        
        LOG_DEBUG("BX/BLX to " + hex32(new_pc));
#if HAS_T32_BL
    } else if (fields.type == INST_T32_BL || fields.alu_op == 1) {
#else
    } else if (fields.alu_op == 1) {
#endif
        // BL instruction (Thumb): PC is current + 4; fields.imm is halfword offset
        int32_t byte_off = static_cast<int32_t>(fields.imm);
        new_pc = current_pc + 4 + byte_off;

        // Save return address in LR (Thumb bit set)
        m_registers->write_register(14, current_pc + 4 + 1);
        m_registers->set_pc(new_pc);
        // Clear any active IT state on control-flow change
        m_registers->clear_it_state();

        LOG_DEBUG("BL to " + hex32(new_pc));
    } else if (fields.type == INST_T32_B || fields.type == INST_T32_B_COND) {
        // T32 branch instructions (B.W and B<cond>.W)
        // Note: immediate already includes the *2 factor from decoder
        int32_t byte_off = static_cast<int32_t>(fields.imm);
        new_pc = current_pc + 4 + byte_off;
        
        m_registers->set_pc(new_pc);
        // Clear any active IT state on control-flow change
        m_registers->clear_it_state();
        LOG_DEBUG("T32 Branch to " + hex32(new_pc));
    } else {
        // Regular T16 branch (B or B<cond>)
        int32_t offset = static_cast<int32_t>(fields.imm);
        new_pc = current_pc + 4 + offset;
        
        m_registers->set_pc(new_pc);
        // Clear any active IT state on control-flow change
        m_registers->clear_it_state();
        LOG_DEBUG("Branch taken to " + hex32(new_pc));
    }
    
    Performance::getInstance().increment_branches_taken();
    return true;
}

bool Execute::execute_data_processing(const InstructionFields& fields)
{
    uint32_t result = 0;
    bool carry = false;
    bool overflow = false;
    uint32_t op1, op2;
    
    // Handle different instruction types
    switch (fields.type) {
        // T16 Immediate operations (Format 3)
        case INST_T16_MOV_IMM: {
            result = fields.imm;
            m_registers->write_register(fields.rd, result);
            update_flags(result, false, false);
            return false;
        }
        case INST_T16_CMP_IMM: {
            op1 = m_registers->read_register(fields.rn);
            result = op1 - fields.imm;
            carry = (op1 >= fields.imm);
            overflow = ((op1 ^ fields.imm) & (op1 ^ result)) >> 31;
            update_flags(result, carry, overflow);
            return false;
        }
        case INST_T16_ADD_IMM8: {
            op1 = m_registers->read_register(fields.rn);
            result = op1 + fields.imm;
            carry = (result < op1);
            overflow = ((op1 ^ result) & (fields.imm ^ result)) >> 31;
            m_registers->write_register(fields.rd, result);
            update_flags(result, carry, overflow);
            return false;
        }
        case INST_T16_SUB_IMM8: {
            op1 = m_registers->read_register(fields.rn);
            result = op1 - fields.imm;
            carry = (op1 >= fields.imm);
            overflow = ((op1 ^ fields.imm) & (op1 ^ result)) >> 31;
            m_registers->write_register(fields.rd, result);
            update_flags(result, carry, overflow);
            return false;
        }
        
        // T16 3-operand operations (Format 2)
        case INST_T16_ADD_REG: {
            op1 = m_registers->read_register(fields.rn);
            op2 = m_registers->read_register(fields.rm);
            result = op1 + op2;
            carry = (result < op1);
            overflow = ((op1 ^ result) & (op2 ^ result)) >> 31;
            m_registers->write_register(fields.rd, result);
            update_flags(result, carry, overflow);
            return false;
        }
        case INST_T16_SUB_REG: {
            op1 = m_registers->read_register(fields.rn);
            op2 = m_registers->read_register(fields.rm);
            result = op1 - op2;
            carry = (op1 >= op2);
            overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
            m_registers->write_register(fields.rd, result);
            update_flags(result, carry, overflow);
            return false;
        }
        case INST_T16_ADD_IMM3: {
            op1 = m_registers->read_register(fields.rn);
            result = op1 + fields.imm;
            carry = (result < op1);
            overflow = ((op1 ^ result) & (fields.imm ^ result)) >> 31;
            m_registers->write_register(fields.rd, result);
            update_flags(result, carry, overflow);
            return false;
        }
        case INST_T16_SUB_IMM3: {
            op1 = m_registers->read_register(fields.rn);
            result = op1 - fields.imm;
            carry = (op1 >= fields.imm);
            overflow = ((op1 ^ fields.imm) & (op1 ^ result)) >> 31;
            m_registers->write_register(fields.rd, result);
            update_flags(result, carry, overflow);
            return false;
        }
        
        // T16 Register operations (Format 4)
        case INST_T16_AND: {
            op1 = m_registers->read_register(fields.rd);
            op2 = m_registers->read_register(fields.rm);
            result = op1 & op2;
            m_registers->write_register(fields.rd, result);
            update_flags(result, false, false);
            return false;
        }
        case INST_T16_EOR: {
            op1 = m_registers->read_register(fields.rd);
            op2 = m_registers->read_register(fields.rm);
            result = op1 ^ op2;
            m_registers->write_register(fields.rd, result);
            update_flags(result, false, false);
            return false;
        }
        case INST_T16_LSL_REG: {
            op1 = m_registers->read_register(fields.rd);
            op2 = m_registers->read_register(fields.rm) & 0xFF;
            if (op2 == 0) {
                result = op1;
                carry = false;
            } else if (op2 < 32) {
                carry = (op1 >> (32 - op2)) & 1;
                result = op1 << op2;
            } else if (op2 == 32) {
                carry = op1 & 1;
                result = 0;
            } else {
                carry = false;
                result = 0;
            }
            m_registers->write_register(fields.rd, result);
            update_flags(result, carry, false);
            return false;
        }
        case INST_T16_LSR_REG: {
            op1 = m_registers->read_register(fields.rd);
            op2 = m_registers->read_register(fields.rm) & 0xFF;
            if (op2 == 0) {
                result = op1;
                carry = false;
            } else if (op2 < 32) {
                carry = (op1 >> (op2 - 1)) & 1;
                result = op1 >> op2;
            } else if (op2 == 32) {
                carry = (op1 >> 31) & 1;
                result = 0;
            } else {
                carry = false;
                result = 0;
            }
            m_registers->write_register(fields.rd, result);
            update_flags(result, carry, false);
            return false;
        }
        case INST_T16_ASR_REG: {
            op1 = m_registers->read_register(fields.rd);
            op2 = m_registers->read_register(fields.rm) & 0xFF;
            if (op2 == 0) {
                result = op1;
                carry = false;
            } else if (op2 < 32) {
                carry = (static_cast<int32_t>(op1) >> (op2 - 1)) & 1;
                result = static_cast<int32_t>(op1) >> op2;
            } else {
                carry = (static_cast<int32_t>(op1) >> 31) & 1;
                result = static_cast<int32_t>(op1) >> 31;
            }
            m_registers->write_register(fields.rd, result);
            update_flags(result, carry, false);
            return false;
        }
        case INST_T16_ADC: {
            op1 = m_registers->read_register(fields.rd);
            op2 = m_registers->read_register(fields.rm);
            uint32_t c_in = m_registers->get_c_flag() ? 1 : 0;
            result = op1 + op2 + c_in;
            carry = (result < op1) || (result == op1 && c_in);
            overflow = ((op1 ^ result) & (op2 ^ result)) >> 31;
            m_registers->write_register(fields.rd, result);
            update_flags(result, carry, overflow);
            return false;
        }
        case INST_T16_SBC: {
            op1 = m_registers->read_register(fields.rd);
            op2 = m_registers->read_register(fields.rm);
            uint32_t c_in = m_registers->get_c_flag() ? 1 : 0;
            result = op1 - op2 - (1 - c_in);
            carry = (op1 >= op2) && (op1 >= (op2 + (1 - c_in)));
            overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
            m_registers->write_register(fields.rd, result);
            update_flags(result, carry, overflow);
            return false;
        }
        case INST_T16_ROR: {
            op1 = m_registers->read_register(fields.rd);
            op2 = m_registers->read_register(fields.rm) & 0xFF;
            if (op2 == 0) {
                result = op1;
                carry = false;
            } else {
                op2 = op2 & 0x1F; // Only bottom 5 bits matter for 32-bit rotate
                if (op2 == 0) {
                    result = op1;
                    carry = (op1 >> 31) & 1;
                } else {
                    result = (op1 >> op2) | (op1 << (32 - op2));
                    carry = (result >> 31) & 1;
                }
            }
            m_registers->write_register(fields.rd, result);
            update_flags(result, carry, false);
            return false;
        }
        case INST_T16_TST: {
            op1 = m_registers->read_register(fields.rn);
            op2 = m_registers->read_register(fields.rm);
            result = op1 & op2;
            update_flags(result, false, false);
            return false;
        }
        case INST_T16_NEG: {
            op2 = m_registers->read_register(fields.rm);
            result = 0 - op2;
            carry = (op2 == 0);
            overflow = (op2 == 0x80000000);
            m_registers->write_register(fields.rd, result);
            update_flags(result, carry, overflow);
            return false;
        }
        case INST_T16_CMP_REG: {
            op1 = m_registers->read_register(fields.rn);
            op2 = m_registers->read_register(fields.rm);
            result = op1 - op2;
            carry = (op1 >= op2);
            overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
            update_flags(result, carry, overflow);
            return false;
        }
        case INST_T16_CMN: {
            op1 = m_registers->read_register(fields.rn);
            op2 = m_registers->read_register(fields.rm);
            result = op1 + op2;
            carry = (result < op1);
            overflow = ((op1 ^ result) & (op2 ^ result)) >> 31;
            update_flags(result, carry, overflow);
            return false;
        }
        case INST_T16_ORR: {
            op1 = m_registers->read_register(fields.rd);
            op2 = m_registers->read_register(fields.rm);
            result = op1 | op2;
            m_registers->write_register(fields.rd, result);
            update_flags(result, false, false);
            return false;
        }
        case INST_T16_MUL: {
            op1 = m_registers->read_register(fields.rd);
            op2 = m_registers->read_register(fields.rm);
            result = op1 * op2;
            m_registers->write_register(fields.rd, result);
            update_flags(result, false, false);
            return false;
        }
        case INST_T16_BIC: {
            op1 = m_registers->read_register(fields.rd);
            op2 = m_registers->read_register(fields.rm);
            result = op1 & (~op2);
            m_registers->write_register(fields.rd, result);
            update_flags(result, false, false);
            return false;
        }
        case INST_T16_MVN: {
            op2 = m_registers->read_register(fields.rm);
            result = ~op2;
            m_registers->write_register(fields.rd, result);
            update_flags(result, false, false);
            return false;
        }
        
        // T16 Hi register operations (Format 5)
        case INST_T16_ADD_HI: {
            op1 = m_registers->read_register(fields.rd);
            op2 = m_registers->read_register(fields.rm);
            result = op1 + op2;
            m_registers->write_register(fields.rd, result);
            // Flags are not affected by ADD (hi)
            return false;
        }
        case INST_T16_CMP_HI: {
            op1 = m_registers->read_register(fields.rd);
            op2 = m_registers->read_register(fields.rm);
            result = op1 - op2;
            carry = (op1 >= op2);
            overflow = ((op1 ^ op2) & (op1 ^ result)) >> 31;
            update_flags(result, carry, overflow);
            return false;
        }
        case INST_T16_MOV_HI: {
            op2 = m_registers->read_register(fields.rm);
            m_registers->write_register(fields.rd, op2);
            // Flags not affected
            return false;
        }
        
        // T16 Shift immediate operations (Format 1)
        case INST_T16_LSL_IMM: {
            op2 = m_registers->read_register(fields.rm);
            if (fields.shift_amount == 0) {
                result = op2;
                carry = false;
            } else {
                carry = (op2 >> (32 - fields.shift_amount)) & 1;
                result = op2 << fields.shift_amount;
            }
            m_registers->write_register(fields.rd, result);
            update_flags(result, carry, false);
            return false;
        }
        case INST_T16_LSR_IMM: {
            op2 = m_registers->read_register(fields.rm);
            uint32_t shift_amt = fields.shift_amount;
            if (shift_amt == 0) shift_amt = 32;
            carry = (op2 >> (shift_amt - 1)) & 1;
            result = op2 >> shift_amt;
            m_registers->write_register(fields.rd, result);
            update_flags(result, carry, false);
            return false;
        }
        case INST_T16_ASR_IMM: {
            op2 = m_registers->read_register(fields.rm);
            uint32_t shift_amt = fields.shift_amount;
            if (shift_amt == 0) shift_amt = 32;
            carry = (static_cast<int32_t>(op2) >> (shift_amt - 1)) & 1;
            result = static_cast<int32_t>(op2) >> shift_amt;
            m_registers->write_register(fields.rd, result);
            update_flags(result, carry, false);
            return false;
        }
        
        // T16 Address generation (Format 12)
        case INST_T16_ADD_PC: {
            uint32_t current_pc = m_registers->get_pc();
            op1 = (current_pc + 4) & ~3;  // PC-relative: align PC to word boundary
            result = op1 + fields.imm;
            m_registers->write_register(fields.rd, result);
            return false;
        }
        case INST_T16_ADD_SP: {
            op1 = m_registers->read_register(13); // SP
            result = op1 + fields.imm;
            m_registers->write_register(fields.rd, result);
            return false;
        }
        
        // T16 SP operations (Format 13)
        case INST_T16_ADD_SP_IMM7: {
            uint32_t sp = m_registers->read_register(13);
            result = sp + fields.imm;
            m_registers->write_register(13, result);
            //LOG_DEBUG("ADD SP, #" + std::to_string(fields.imm) + " -> SP=" + hex32(result));
            return false;
        }
        case INST_T16_SUB_SP_IMM7: {
            uint32_t sp = m_registers->read_register(13);
            result = sp - fields.imm;
            m_registers->write_register(13, result);
            //LOG_DEBUG("SUB SP, #" + std::to_string(fields.imm) + " -> SP=" + hex32(result));
            return false;
        }
        
        default:
            // Unknown instruction type for data processing
            LOG_WARNING("Unknown data processing instruction type: " + std::to_string(fields.type));
            return false;
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
    LOG_DEBUG("Load: " + reg_name(fields.rd) + " = [" + hex32(address) + "] = " + hex32(data));
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
        
    LOG_DEBUG("Store: [" + hex32(address) + "] = " + reg_name(fields.rd) + " = " + hex32(data));
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
                                // Clear IT state on control-flow change via exception return
                                m_registers->clear_it_state();
                                return true; // PC changed by exception return
                            }
                            // If exception return failed, fall through to normal branch
                        } else {
                            // Normal POP to PC
                            m_registers->set_pc(data & ~1u); // Clear Thumb bit
                            address += 4;
                            m_registers->write_register(13, address); // Update SP
                            // Clear IT state on control-flow change
                            m_registers->clear_it_state();
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

    // Handle BKPT instruction
    if (fields.type == INST_T16_BKPT) {
        LOG_INFO("BKPT instruction executed with immediate value: " + hex32(fields.imm));
        // In a real system, this would trigger a debug exception
        // For simulation, we just log it and continue
        return false;
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
    // Clear IT state when entering exception
    m_registers->clear_it_state();
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
    uint32_t result = 0;

#if SUPPORTS_ARMV7_M
    // Check if this is a T32 extend instruction or T16 extend instruction
    if (fields.type == INST_T32_SXTH || fields.type == INST_T32_SXTB || 
        fields.type == INST_T32_UXTH || fields.type == INST_T32_UXTB) {
        // T32 extend instructions use fields.rn as source
        uint32_t source_value = m_registers->read_register(fields.rn);
        
        switch (fields.type) {
            case INST_T32_SXTH: {
                // Sign extend halfword (16-bit) to word (32-bit)
                uint16_t halfword = static_cast<uint16_t>(source_value & 0xFFFF);
                result = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(halfword)));
                LOG_DEBUG("SXTH.W: sign extended " + hex32(halfword) + " to " + hex32(result));
                break;
            }
            
            case INST_T32_SXTB: {
                // Sign extend byte (8-bit) to word (32-bit)
                uint8_t byte = static_cast<uint8_t>(source_value & 0xFF);
                result = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int8_t>(byte)));
                LOG_DEBUG("SXTB.W: sign extended " + hex32(byte) + " to " + hex32(result));
                break;
            }
            
            case INST_T32_UXTH: {
                // Zero extend halfword (16-bit) to word (32-bit)
                result = source_value & 0xFFFF;
                LOG_DEBUG("UXTH.W: zero extended to " + hex32(result));
                break;
            }
            
            case INST_T32_UXTB: {
                // Zero extend byte (8-bit) to word (32-bit)
                result = source_value & 0xFF;
                LOG_DEBUG("UXTB.W: zero extended to " + hex32(result));
                break;
            }
        }
    } else 
#endif
    {
        // T16 extend instructions use fields.rm as source and fields.alu_op to distinguish
        uint32_t rm_val = m_registers->read_register(fields.rm);
        
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
        
        LOG_DEBUG("Extend: " + reg_name(fields.rd) + " = extend(R" + reg_name(fields.rm) + 
                  ") = " + hex32(result));
    }
    
    m_registers->write_register(fields.rd, result);
    return false;
}

bool Execute::execute_rev(const InstructionFields& fields)
{
    uint32_t source_value = m_registers->read_register(fields.rm);
    uint32_t result = 0;
    
    switch (fields.type) {
        case INST_T16_REV: {
            // Reverse byte order in word
            result = ((source_value & 0xFF000000) >> 24) |
                     ((source_value & 0x00FF0000) >> 8)  |
                     ((source_value & 0x0000FF00) << 8)  |
                     ((source_value & 0x000000FF) << 24);
            LOG_DEBUG("REV: " + hex32(source_value) + " -> " + hex32(result));
            break;
        }
        case INST_T16_REV16: {
            // Reverse bytes in each halfword
            result = ((source_value & 0xFF000000) >> 8)  |
                     ((source_value & 0x00FF0000) << 8)  |
                     ((source_value & 0x0000FF00) >> 8)  |
                     ((source_value & 0x000000FF) << 8);
            LOG_DEBUG("REV16: " + hex32(source_value) + " -> " + hex32(result));
            break;
        }
        case INST_T16_REVSH: {
            // Reverse bytes in bottom halfword and sign extend to 32 bits
            uint16_t halfword = source_value & 0xFFFF;
            uint16_t reversed = ((halfword & 0xFF00) >> 8) | ((halfword & 0x00FF) << 8);
            result = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(reversed)));
            LOG_DEBUG("REVSH: " + hex32(source_value) + " -> " + hex32(result));
            break;
        }
        default:
            LOG_ERROR("Unknown REV instruction type: " + std::to_string(fields.type));
            return false;
    }
    
    m_registers->write_register(fields.rd, result);
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
        } else {
            // CPSIE I - enable interrupts (set PRIMASK = 0) 
            m_registers->enable_interrupts();
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
            break;
            
        case 0x14: // CONTROL 
            reg_name = "CONTROL";
            m_registers->set_control(source_value);
            break;
            
        case 0x00: // APSR (Application Program Status Register)
            {
                reg_name = "APSR";
                // Only allow updating of condition flags (bits 31-28)
                uint32_t current_psr = m_registers->get_psr();
                uint32_t new_psr = (current_psr & ~0xF0000000) | (source_value & 0xF0000000);
                m_registers->set_psr(new_psr);
                break;
            }
            
        case 0x10: // PRIMASK
            {
                reg_name = "PRIMASK";
                m_registers->set_primask(source_value);
                bool interrupts_enabled = m_registers->interrupts_enabled();
                break;
            }
        case 0x08: // MSP
            {
                reg_name = "MSP";
                m_registers->set_msp(source_value);
                break;
            }
        case 0x11: // BASEPRI
            {
                reg_name = "BASEPRI";
                m_registers->set_basepri(source_value);
                break;
            }
        case 0x12: // BASEPRI_MAX
            {
                reg_name = "BASEPRI_MAX";
                // BASEPRI_MAX only updates BASEPRI if new value is non-zero and higher priority
                uint32_t current_basepri = m_registers->get_basepri();
                if (source_value != 0 && (current_basepri == 0 || source_value < current_basepri)) {
                    m_registers->set_basepri(source_value);
                }
                break;
            }
        case 0x13: // FAULTMASK
            {
                reg_name = "FAULTMASK";
                m_registers->set_faultmask(source_value);
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
            
        case 0x11: // BASEPRI
            reg_name = "BASEPRI";
            value = m_registers->get_basepri();
            break;
            
        case 0x12: // BASEPRI_MAX (reads current BASEPRI value)
            reg_name = "BASEPRI_MAX";
            value = m_registers->get_basepri();
            break;
            
        case 0x13: // FAULTMASK
            reg_name = "FAULTMASK";
            value = m_registers->get_faultmask();
            break;
            
        default:
            reg_name = "UNKNOWN(" + std::to_string(spec_reg) + ")";
            LOG_WARNING("mrs from unknown special register: " + std::to_string(spec_reg));
            value = 0;
            break;
    }
    
    m_registers->write_register(fields.rd, value);
    //LOG_DEBUG("mrs r" + std::to_string(fields.rd) + ", " + reg_name + " = " + hex32(value));

    return false;
}

#if HAS_CBZ_CBNZ
bool Execute::execute_cbz_cbnz(const InstructionFields& fields)
{
    // CBZ/CBNZ - Compare and Branch Zero/Non-Zero
    uint32_t rn_value = m_registers->read_register(fields.rn);
    bool should_branch = false;
    
    if (fields.type == INST_T16_CBZ) {
        // CBZ - branch if register is zero
        should_branch = (rn_value == 0);
        LOG_DEBUG("CBZ R" + std::to_string(fields.rn) + " (=" + hex32(rn_value) + "), target=PC+" + std::to_string(fields.imm));
    } else {
        // CBNZ - branch if register is non-zero  
        should_branch = (rn_value != 0);
        LOG_DEBUG("CBNZ R" + std::to_string(fields.rn) + " (=" + hex32(rn_value) + "), target=PC+" + std::to_string(fields.imm));
    }
    
    if (should_branch) {
        uint32_t current_pc = m_registers->get_pc();
        uint32_t target_pc = current_pc + 4 + fields.imm; // PC+4 + offset
        m_registers->set_pc(target_pc);
        // Clear IT state on control-flow change
        m_registers->clear_it_state();
        LOG_DEBUG("CBZ/CBNZ branch taken to " + hex32(target_pc));
        Performance::getInstance().increment_branches_taken();
        return true; // PC changed
    }
    
    return false; // PC not changed
}
#endif

#if HAS_IT_BLOCKS
bool Execute::execute_it(const InstructionFields& fields)
{
    // IT - If-Then conditional execution block
    uint8_t firstcond = static_cast<uint8_t>(fields.cond & 0xF);
    uint8_t mask = static_cast<uint8_t>(fields.imm & 0xF);

    // Decode IT instruction format:
    // IT{x{y{z}}} cond
    // mask bits: [3:0] = xyz1 where 1 means "then", 0 means "else"
    // bit 3 is always 1 (marks end of IT block)
    
    if (mask == 0) {
        // This shouldn't happen as mask=0 would be a different instruction
        LOG_ERROR("Invalid IT instruction with mask=0");
        return false;
    }

    // Initialize IT state and derived pattern
    m_registers->start_it(static_cast<uint8_t>(firstcond & 0xF), static_cast<uint8_t>(mask & 0xF));
    
    // Evaluate condition once at IT block start and store the result
    bool cond_result = check_condition(firstcond);
    m_registers->set_it_condition_result(cond_result);

    return false;
}
#endif

#if HAS_EXTENDED_HINTS
bool Execute::execute_extended_hint(const InstructionFields& fields)
{
    // ARMv7-M Extended Hint instructions
    switch (fields.type) {
        case INST_T16_NOP:
            LOG_DEBUG("nop - No Operation");
            // Hint to scheduler that this thread can yield
            wait(1, SC_NS); // Minimal wait to allow other processes
            break;

        case INST_T16_WFI:
            LOG_DEBUG("WFI - Wait for Interrupt");
            // In real implementation: put core into low-power state until interrupt
            wait(100, SC_NS); // Simulate brief wait
            break;
            
        case INST_T16_WFE:
            LOG_DEBUG("WFE - Wait for Event");
            // In real implementation: put core into low-power state until event
            wait(50, SC_NS); // Simulate brief wait
            break;
            
        case INST_T16_SEV:
            LOG_DEBUG("SEV - Send Event");
            // In real implementation: send event to other cores/wake WFE
            // For single-core simulation, this is essentially a NOP
            break;
            
        case INST_T16_YIELD:
            LOG_DEBUG("YIELD - Yield processor");
            // Hint to scheduler that this thread can yield
            wait(1, SC_NS); // Minimal wait to allow other processes
            break;
            
        default:
            LOG_WARNING("Unknown extended hint instruction: " + std::to_string(fields.type));
            break;
    }
    
    return false;
}
#endif

#if SUPPORTS_ARMV7_M
bool Execute::execute_table_branch(const InstructionFields& fields, void* data_bus)
{
    // TBB/TBH - Table Branch Byte/Halfword
    // Used for efficient switch statement implementation
    
    uint32_t base_addr = m_registers->read_register(fields.rn);
    uint32_t index = m_registers->read_register(fields.rm);
    uint32_t current_pc = m_registers->get_pc();
    
    uint32_t offset;
    if (fields.type == INST_T32_TBB) {
        // TBB - Table Branch Byte
        uint32_t table_addr = base_addr + index;
        uint8_t branch_offset = (uint8_t)read_memory(table_addr, 1, data_bus);
        offset = branch_offset * 2; // Thumb instructions are 2-byte aligned
        LOG_DEBUG("TBB: base=" + hex32(base_addr) + ", index=" + std::to_string(index) + 
                 ", offset=" + std::to_string(branch_offset));
    } else {
        // TBH - Table Branch Halfword  
        uint32_t table_addr = base_addr + (index * 2);
        uint16_t branch_offset = (uint16_t)read_memory(table_addr, 2, data_bus);
        offset = branch_offset * 2; // Thumb instructions are 2-byte aligned
        LOG_DEBUG("TBH: base=" + hex32(base_addr) + ", index=" + std::to_string(index) + 
                 ", offset=" + std::to_string(branch_offset));
    }
    
    // Branch to PC + 4 + 2*offset (PC+4 because PC has already advanced)
    uint32_t target_pc = current_pc + 4 + offset;
    m_registers->set_pc(target_pc);
    // Clear IT state on control-flow change
    m_registers->clear_it_state();
    
    LOG_DEBUG("Table branch to PC=" + hex32(target_pc));
    return true; // PC changed
}

bool Execute::execute_clrex(const InstructionFields& fields)
{
    // CLREX - Clear Exclusive monitor
    // Used in conjunction with LDREX/STREX for atomic operations
    
    LOG_DEBUG("CLREX - Clear Exclusive monitor");
    
#if HAS_EXCLUSIVE_ACCESS
    // Clear the exclusive monitor state
    m_exclusive_monitor_enabled = false;
    m_exclusive_address = 0;
    m_exclusive_size = 0;
#endif
    
    return false;
}

bool Execute::execute_t32_data_processing(const InstructionFields& fields)
{
    // T32 Data Processing instructions with immediate operands
    
    // Check condition code if this is a conditional instruction
    if (fields.cond != 0xE && fields.cond != 0x0) { // Not AL (always) or EQ
        if (!check_condition(fields.cond)) {
            LOG_DEBUG("T32 Data Processing: Condition not met (cond=" + std::to_string(fields.cond) + ")");
            return false;  // Condition not met, skip execution
        }
    }
    
    uint32_t operand1 = 0;
    // Read operand1 (rn) for instructions that need it - most do except MOV/MVN
    switch (fields.type) {
        case INST_T32_MOV_IMM:
        case INST_T32_MOVS_IMM:
        case INST_T32_MVN_IMM:
        case INST_T32_MOVW:
            // These instructions don't use rn
            break;
        case INST_T32_MOVT:
            // MOVT reads the current value of rd, not rn
            operand1 = m_registers->read_register(fields.rd);
            break;
        default:
            // Most instructions (ADD, SUB, etc.) use rn
            operand1 = read_register_with_pc_adjust(m_registers, fields.rn);
            break;
    }
    uint32_t operand2 = fields.imm;
    uint32_t result = 0;
    bool carry = false;
    bool overflow = false;
    
    switch (fields.type) {
        case INST_T32_MOV_IMM:
            result = operand2;
            LOG_DEBUG("MOV.W " + reg_name(fields.rd) + ", #" + hex32(operand2) + 
                     " -> " + hex32(result));
            break;
            
        case INST_T32_MOVS_IMM:
            result = operand2;
            LOG_DEBUG("MOVS.W " + reg_name(fields.rd) + ", #" + hex32(operand2) + 
                     " -> " + hex32(result));
            break;
            
        case INST_T32_MVN_IMM:
            result = ~operand2;
            LOG_DEBUG("MVN.W " + reg_name(fields.rd) + ", #" + hex32(operand2) + 
                     " -> " + hex32(result));
            break;
            
        case INST_T32_ADD_IMM:
            result = operand1 + operand2;
            carry = result < operand1;  // Carry on unsigned overflow
            overflow = ((operand1 ^ result) & (operand2 ^ result) & 0x80000000) != 0;
            LOG_DEBUG("ADD.W " + reg_name(fields.rd) + ", " + reg_name(fields.rn) + 
                     ", #" + hex32(operand2) + " -> " + hex32(result));
            break;
            
        case INST_T32_SUB_IMM:
            result = operand1 - operand2;
            carry = operand1 >= operand2;  // No borrow
            overflow = ((operand1 ^ operand2) & (operand1 ^ result) & 0x80000000) != 0;
            LOG_DEBUG("SUB.W " + reg_name(fields.rd) + ", " + reg_name(fields.rn) + 
                     ", #" + hex32(operand2) + " -> " + hex32(result));
            break;

        case INST_T32_ADDW:
            result = operand1 + operand2;
            // ADDW doesn't set flags, so no carry/overflow calculation needed
            LOG_DEBUG("ADDW " + reg_name(fields.rd) + ", " + 
                     (fields.rn == 15 ? "pc" : "r" + reg_name(fields.rn)) + 
                     ", #" + std::to_string(operand2) + " -> " + hex32(result));
            break;
            
        case INST_T32_SUBW:
            result = operand1 - operand2;
            // SUBW doesn't set flags, so no carry/overflow calculation needed
            LOG_DEBUG("SUBW " + reg_name(fields.rd) + ", " + 
                     (fields.rn == 15 ? "pc" : "r" + reg_name(fields.rn)) + 
                     ", #" + std::to_string(operand2) + " -> " + hex32(result));
            break;

        case INST_T32_MOVW:
            result = fields.imm; // MOVW just moves the 16-bit immediate to the register
            // MOVW doesn't set flags
            LOG_DEBUG("MOVW " + reg_name(fields.rd) + ", #" + std::to_string(fields.imm) + 
                     " -> " + hex32(result));
            break;
            
        case INST_T32_MOVT:
            // MOVT moves the 16-bit immediate to upper 16 bits, preserving lower 16 bits
            result = (operand1 & 0xFFFF) | (fields.imm << 16);
            // MOVT doesn't set flags
            LOG_DEBUG("MOVT " + reg_name(fields.rd) + ", #" + std::to_string(fields.imm) + 
                     " -> " + hex32(result));
            break;

        case INST_T32_ADC_IMM: {
            uint32_t apsr = m_registers->get_apsr();
            uint32_t carry_in = (apsr & 0x20000000) ? 1 : 0;
            result = operand1 + operand2 + carry_in;
            carry = (result < operand1) || (carry_in && result == operand1);
            overflow = ((operand1 ^ result) & (operand2 ^ result) & 0x80000000) != 0;
            LOG_DEBUG("ADC.W " + reg_name(fields.rd) + ", " + reg_name(fields.rn) + 
                     ", #" + hex32(operand2) + " -> " + hex32(result));
            break;
        }

        case INST_T32_SBC_IMM: {
            uint32_t apsr = m_registers->get_apsr();
            uint32_t carry_in = (apsr & 0x20000000) ? 1 : 0;
            result = operand1 - operand2 - (1 - carry_in);
            carry = (operand1 >= operand2) && (carry_in || operand1 > operand2);
            overflow = ((operand1 ^ operand2) & (operand1 ^ result) & 0x80000000) != 0;
            LOG_DEBUG("SBC.W " + reg_name(fields.rd) + ", " + reg_name(fields.rn) + 
                     ", #" + hex32(operand2) + " -> " + hex32(result));
            break;
        }

        case INST_T32_RSB_IMM:
            result = operand2 - operand1;
            carry = operand2 >= operand1;
            overflow = ((operand2 ^ operand1) & (operand2 ^ result) & 0x80000000) != 0;
            LOG_DEBUG("RSB.W " + reg_name(fields.rd) + ", " + reg_name(fields.rn) + 
                     ", #" + hex32(operand2) + " -> " + hex32(result));
            break;
            
        case INST_T32_AND_IMM:
            result = operand1 & operand2;
            LOG_DEBUG("AND.W " + reg_name(fields.rd) + ", " + reg_name(fields.rn) + 
                     ", #" + hex32(operand2) + " -> " + hex32(result));
            break;
            
        case INST_T32_ORR_IMM:
            result = operand1 | operand2;
            LOG_DEBUG("ORR.W " + reg_name(fields.rd) + ", " + reg_name(fields.rn) + 
                     ", #" + hex32(operand2) + " -> " + hex32(result));
            break;
            
        case INST_T32_EOR_IMM:
            result = operand1 ^ operand2;
            LOG_DEBUG("EOR.W " + reg_name(fields.rd) + ", " + reg_name(fields.rn) + 
                     ", #" + hex32(operand2) + " -> " + hex32(result));
            break;
            
        case INST_T32_BIC_IMM:
            result = operand1 & (~operand2);
            LOG_DEBUG("BIC.W " + reg_name(fields.rd) + ", " + reg_name(fields.rn) + 
                     ", #" + hex32(operand2) + " -> " + hex32(result));
            break;
            
        case INST_T32_ORN_IMM:
            result = operand1 | (~operand2);
            LOG_DEBUG("ORN.W " + reg_name(fields.rd) + ", " + reg_name(fields.rn) + 
                     ", #" + hex32(operand2) + " -> " + hex32(result));
            break;
            
        // Compare instructions (no result register)
        case INST_T32_CMP_IMM:
            result = operand1 - operand2;
            carry = operand1 >= operand2;
            overflow = ((operand1 ^ operand2) & (operand1 ^ result) & 0x80000000) != 0;
            LOG_DEBUG("CMP.W " + reg_name(fields.rn) + ", #" + hex32(operand2));
            update_flags(result, carry, overflow);
            return false;
            
        case INST_T32_CMN_IMM:
            result = operand1 + operand2;
            carry = result < operand1;
            overflow = ((operand1 ^ result) & (operand2 ^ result) & 0x80000000) != 0;
            LOG_DEBUG("CMN.W " + reg_name(fields.rn) + ", #" + hex32(operand2));
            update_flags(result, carry, overflow);
            return false;
            
        case INST_T32_TST_IMM:
            result = operand1 & operand2;
            LOG_DEBUG("TST.W " + reg_name(fields.rn) + ", #" + hex32(operand2));
            update_flags(result, false, false);
            return false;
            
        case INST_T32_TEQ_IMM:
            result = operand1 ^ operand2;
            LOG_DEBUG("TEQ.W " + reg_name(fields.rn) + ", #" + hex32(operand2));
            update_flags(result, false, false);
            return false;

        // T32 Data Processing Instructions (Register operands)
        case INST_T32_AND_REG:
        case INST_T32_ANDS_REG: {
            uint32_t operand2_shifted = apply_shift(read_register_with_pc_adjust(m_registers, fields.rm), 
                                                   fields.shift_type, fields.shift_amount);
            result = operand1 & operand2_shifted;
            bool set_flags = (fields.type == INST_T32_ANDS_REG);
            LOG_DEBUG("AND.W " + reg_name(fields.rd) + 
                     ", " + reg_name(fields.rn) + ", " + reg_name(fields.rm) + 
                     " -> " + hex32(result));
            if (set_flags) {
                update_flags(result, false, false);
            }
            break;
        }
            
        case INST_T32_ORR_REG:
        case INST_T32_ORRS_REG: {
            uint32_t operand2_shifted = apply_shift(read_register_with_pc_adjust(m_registers, fields.rm), 
                                                   fields.shift_type, fields.shift_amount);
            result = operand1 | operand2_shifted;
            bool set_flags = (fields.type == INST_T32_ORRS_REG);
            LOG_DEBUG("ORR.W " + reg_name(fields.rd) + 
                     ", " + reg_name(fields.rn) + ", " + reg_name(fields.rm) + 
                     " -> " + hex32(result));
            if (set_flags) {
                update_flags(result, false, false);
            }
            break;
        }
            
        case INST_T32_EOR_REG:
        case INST_T32_EORS_REG: {
            uint32_t operand2_shifted = apply_shift(read_register_with_pc_adjust(m_registers, fields.rm), 
                                                   fields.shift_type, fields.shift_amount);
            result = operand1 ^ operand2_shifted;
            bool set_flags = (fields.type == INST_T32_EORS_REG);
            LOG_DEBUG("EOR.W " + reg_name(fields.rd) + 
                     ", " + reg_name(fields.rn) + ", " + reg_name(fields.rm) + 
                     " -> " + hex32(result));
            if (set_flags) {
                update_flags(result, false, false);
            }
            break;
        }
            
        case INST_T32_BIC_REG:
        case INST_T32_BICS_REG: {
            uint32_t operand2_shifted = apply_shift(read_register_with_pc_adjust(m_registers, fields.rm), 
                                                   fields.shift_type, fields.shift_amount);
            result = operand1 & (~operand2_shifted);
            bool set_flags = (fields.type == INST_T32_BICS_REG);
            LOG_DEBUG("BIC.W " + reg_name(fields.rd) + 
                     ", " + reg_name(fields.rn) + ", " + reg_name(fields.rm) + 
                     " -> " + hex32(result));
            if (set_flags) {
                update_flags(result, false, false);
            }
            break;
        }
            
        case INST_T32_ORN_REG:
        case INST_T32_ORNS_REG: {
            uint32_t operand2_shifted = apply_shift(read_register_with_pc_adjust(m_registers, fields.rm), 
                                                   fields.shift_type, fields.shift_amount);
            result = operand1 | (~operand2_shifted);
            bool set_flags = (fields.type == INST_T32_ORNS_REG);
            LOG_DEBUG("ORN.W " + reg_name(fields.rd) + 
                     ", " + reg_name(fields.rn) + ", " + reg_name(fields.rm) + 
                     " -> " + hex32(result));
            if (set_flags) {
                update_flags(result, false, false);
            }
            break;
        }
            
        case INST_T32_ADD_REG:
        case INST_T32_ADDS_REG: {
            uint32_t operand2_shifted = apply_shift(read_register_with_pc_adjust(m_registers, fields.rm), 
                                                   fields.shift_type, fields.shift_amount);
            result = operand1 + operand2_shifted;
            carry = result < operand1;
            overflow = ((operand1 ^ result) & (operand2_shifted ^ result) & 0x80000000) != 0;
            bool set_flags = (fields.type == INST_T32_ADDS_REG);
            LOG_DEBUG("ADD.W " + reg_name(fields.rd) + 
                     ", " + reg_name(fields.rn) + ", " + reg_name(fields.rm) + 
                     " -> " + hex32(result));
            if (set_flags) {
                update_flags(result, carry, overflow);
            }
            break;
        }
            
        case INST_T32_SUB_REG:
        case INST_T32_SUBS_REG: {
            uint32_t operand2_shifted = apply_shift(read_register_with_pc_adjust(m_registers, fields.rm), 
                                                   fields.shift_type, fields.shift_amount);
            result = operand1 - operand2_shifted;
            carry = operand1 >= operand2_shifted;
            overflow = ((operand1 ^ operand2_shifted) & (operand1 ^ result) & 0x80000000) != 0;
            bool set_flags = (fields.type == INST_T32_SUBS_REG);
            LOG_DEBUG("SUB.W " + reg_name(fields.rd) + 
                     ", " + reg_name(fields.rn) + ", " + reg_name(fields.rm) + 
                     " -> " + hex32(result));
            if (set_flags) {
                update_flags(result, carry, overflow);
            }
            break;
        }
            
        case INST_T32_ADC_REG:
        case INST_T32_ADCS_REG: {
            uint32_t operand2_shifted = apply_shift(read_register_with_pc_adjust(m_registers, fields.rm), 
                                                   fields.shift_type, fields.shift_amount);
            uint32_t apsr = m_registers->get_apsr();
            uint32_t carry_in = (apsr & 0x20000000) ? 1 : 0;
            result = operand1 + operand2_shifted + carry_in;
            carry = (result < operand1) || (carry_in && result == operand1);
            overflow = ((operand1 ^ result) & (operand2_shifted ^ result) & 0x80000000) != 0;
            bool set_flags = (fields.type == INST_T32_ADCS_REG);
            LOG_DEBUG("ADC.W " + reg_name(fields.rd) + 
                     ", " + reg_name(fields.rn) + ", " + reg_name(fields.rm) + 
                     " -> " + hex32(result));
            if (set_flags) {
                update_flags(result, carry, overflow);
            }
            break;
        }
            
        case INST_T32_SBC_REG:
        case INST_T32_SBCS_REG: {
            uint32_t operand2_shifted = apply_shift(read_register_with_pc_adjust(m_registers, fields.rm), 
                                                   fields.shift_type, fields.shift_amount);
            uint32_t apsr = m_registers->get_apsr();
            uint32_t carry_in = (apsr & 0x20000000) ? 1 : 0;
            result = operand1 - operand2_shifted - (1 - carry_in);
            carry = (operand1 >= operand2_shifted) && (carry_in || operand1 > operand2_shifted);
            overflow = ((operand1 ^ operand2_shifted) & (operand1 ^ result) & 0x80000000) != 0;
            bool set_flags = (fields.type == INST_T32_SBCS_REG);
            LOG_DEBUG("SBC.W " + reg_name(fields.rd) + 
                     ", " + reg_name(fields.rn) + ", " + reg_name(fields.rm) + 
                     " -> " + hex32(result));
            if (set_flags) {
                update_flags(result, carry, overflow);
            }
            break;
        }
            
        case INST_T32_RSB_REG:
        case INST_T32_RSBS_REG: {
            uint32_t operand2_shifted = apply_shift(read_register_with_pc_adjust(m_registers, fields.rm), 
                                                   fields.shift_type, fields.shift_amount);
            result = operand2_shifted - operand1;
            carry = operand2_shifted >= operand1;
            overflow = ((operand2_shifted ^ operand1) & (operand2_shifted ^ result) & 0x80000000) != 0;
            bool set_flags = (fields.type == INST_T32_RSBS_REG);
            LOG_DEBUG("RSB.W " + reg_name(fields.rd) + 
                     ", " + reg_name(fields.rn) + ", " + reg_name(fields.rm) + 
                     " -> " + hex32(result));
            if (set_flags) {
                update_flags(result, carry, overflow);
            }
            break;
        }
            
        // MOV instructions (single operand)
        case INST_T32_MOV_REG:
        case INST_T32_MOVS_REG: {
            uint32_t operand2_shifted = apply_shift(read_register_with_pc_adjust(m_registers, fields.rm), 
                                                   fields.shift_type, fields.shift_amount);
            result = operand2_shifted;
            bool set_flags = (fields.type == INST_T32_MOVS_REG);
            LOG_DEBUG("MOV.W " + reg_name(fields.rd) + 
                     ", " + reg_name(fields.rm) + 
                     " -> " + hex32(result));
            if (set_flags) {
                update_flags(result, false, false);
            }
            break;
        }
            
        case INST_T32_MVN_REG:
        case INST_T32_MVNS_REG: {
            uint32_t operand2_shifted = apply_shift(read_register_with_pc_adjust(m_registers, fields.rm), 
                                                   fields.shift_type, fields.shift_amount);
            result = ~operand2_shifted;
            bool set_flags = (fields.type == INST_T32_MVNS_REG);
            LOG_DEBUG("MVN.W " + reg_name(fields.rd) + 
                     ", " + reg_name(fields.rm) + 
                     " -> " + hex32(result));
            if (set_flags) {
                update_flags(result, false, false);
            }
            break;
        }
            
        // Compare instructions (no result register)
        case INST_T32_TST_REG: {
            uint32_t operand2_shifted = apply_shift(read_register_with_pc_adjust(m_registers, fields.rm), 
                                                   fields.shift_type, fields.shift_amount);
            result = operand1 & operand2_shifted;
            LOG_DEBUG("TST.W " + reg_name(fields.rn) + ", " + reg_name(fields.rm));
            update_flags(result, false, false);
            return false;
        }
            
        case INST_T32_TEQ_REG: {
            uint32_t operand2_shifted = apply_shift(read_register_with_pc_adjust(m_registers, fields.rm), 
                                                   fields.shift_type, fields.shift_amount);
            result = operand1 ^ operand2_shifted;
            LOG_DEBUG("TEQ.W " + reg_name(fields.rn) + ", " + reg_name(fields.rm));
            update_flags(result, false, false);
            return false;
        }
            
        case INST_T32_CMP_REG: {
            uint32_t operand2_shifted = apply_shift(read_register_with_pc_adjust(m_registers, fields.rm), 
                                                   fields.shift_type, fields.shift_amount);
            result = operand1 - operand2_shifted;
            carry = operand1 >= operand2_shifted;
            overflow = ((operand1 ^ operand2_shifted) & (operand1 ^ result) & 0x80000000) != 0;
            LOG_DEBUG("CMP.W " + reg_name(fields.rn) + ", " + reg_name(fields.rm));
            update_flags(result, carry, overflow);
            return false;
        }
            
        case INST_T32_CMN_REG: {
            uint32_t operand2_shifted = apply_shift(read_register_with_pc_adjust(m_registers, fields.rm), 
                                                   fields.shift_type, fields.shift_amount);
            result = operand1 + operand2_shifted;
            carry = result < operand1;
            overflow = ((operand1 ^ result) & (operand2_shifted ^ result) & 0x80000000) != 0;
            LOG_DEBUG("CMN.W " + reg_name(fields.rn) + ", " + reg_name(fields.rm));
            update_flags(result, carry, overflow);
            return false;
        }
            
        default:
            LOG_WARNING("Unknown T32 data processing instruction: " + std::to_string(fields.type));
            return false;
    }
    
    // Write result to destination register (except for compare instructions)
    if (fields.rd < 16) {
        m_registers->write_register(fields.rd, result);
    }
    
    // Update flags if S bit is set
    if (fields.s_bit) {
        update_flags(result, carry, overflow);
    }
    
    return false;
}

bool Execute::execute_t32_shift_register(const InstructionFields& fields)
{
    // T32 Shift instructions with register operands
    // LSL.W/LSR.W/ASR.W/ROR.W Rd, Rm, Rs
    
    uint32_t rm_value = m_registers->read_register(fields.rm);
    uint32_t rs_value = m_registers->read_register(fields.rn) & 0xFF; // Only bottom 8 bits used
    uint32_t result = 0;
    bool carry_out = false;
    
    // Get current carry flag for shift operations
    bool carry_in = m_registers->get_c_flag();
    
    switch (fields.type) {
        case INST_T32_LSL_REG:
        case INST_T32_LSLS_REG:
            if (rs_value == 0) {
                result = rm_value;
                carry_out = carry_in;
            } else if (rs_value < 32) {
                carry_out = (rm_value >> (32 - rs_value)) & 1;
                result = rm_value << rs_value;
            } else if (rs_value == 32) {
                carry_out = rm_value & 1;
                result = 0;
            } else {
                carry_out = false;
                result = 0;
            }
            LOG_DEBUG("LSL.W " + reg_name(fields.rd) + ", " + reg_name(fields.rm) + 
                     ", " + reg_name(fields.rn) + " (shift=" + std::to_string(rs_value) + 
                     ") -> " + hex32(result));
            break;
            
        case INST_T32_LSR_REG:
        case INST_T32_LSRS_REG:
            if (rs_value == 0) {
                result = rm_value;
                carry_out = carry_in;
            } else if (rs_value < 32) {
                carry_out = (rm_value >> (rs_value - 1)) & 1;
                result = rm_value >> rs_value;
            } else if (rs_value == 32) {
                carry_out = (rm_value >> 31) & 1;
                result = 0;
            } else {
                carry_out = false;
                result = 0;
            }
            LOG_DEBUG("LSR.W " + reg_name(fields.rd) + ", " + reg_name(fields.rm) + 
                     ", " + reg_name(fields.rn) + " (shift=" + std::to_string(rs_value) + 
                     ") -> " + hex32(result));
            break;
            
        case INST_T32_ASR_REG:
        case INST_T32_ASRS_REG:
            if (rs_value == 0) {
                result = rm_value;
                carry_out = carry_in;
            } else if (rs_value < 32) {
                carry_out = (rm_value >> (rs_value - 1)) & 1;
                result = static_cast<uint32_t>(static_cast<int32_t>(rm_value) >> rs_value);
            } else {
                carry_out = (rm_value >> 31) & 1;
                result = (rm_value & 0x80000000) ? 0xFFFFFFFF : 0;
            }
            LOG_DEBUG("ASR.W " + reg_name(fields.rd) + ", " + reg_name(fields.rm) + 
                     ", " + reg_name(fields.rn) + " (shift=" + std::to_string(rs_value) + 
                     ") -> " + hex32(result));
            break;
            
        case INST_T32_ROR_REG:
        case INST_T32_RORS_REG:
            if (rs_value == 0) {
                result = rm_value;
                carry_out = carry_in;
            } else {
                rs_value = rs_value & 31; // ROR uses bottom 5 bits
                if (rs_value == 0) {
                    result = rm_value;
                    carry_out = (rm_value >> 31) & 1;
                } else {
                    result = (rm_value >> rs_value) | (rm_value << (32 - rs_value));
                    carry_out = (result >> 31) & 1;
                }
            }
            LOG_DEBUG("ROR.W " + reg_name(fields.rd) + ", " + reg_name(fields.rm) + 
                     ", " + reg_name(fields.rn) + " (shift=" + std::to_string(rs_value) + 
                     ") -> " + hex32(result));
            break;
            
        default:
            LOG_ERROR("Unknown T32 shift instruction: " + std::to_string(fields.type));
            return false;
    }
    
    // Write result to destination register
    m_registers->write_register(fields.rd, result);
    
    // Update flags if S bit is set
    if (fields.s_bit) {
        m_registers->set_n_flag((result & 0x80000000) != 0);
        m_registers->set_z_flag(result == 0);
        m_registers->set_c_flag(carry_out);
        // V flag is unaffected by shift operations
    }
    
    return false;
}

bool Execute::execute_t32_load_store(const InstructionFields& fields, void* data_bus)
{
    // T32 Load/Store instructions
    
    uint32_t address;
    uint32_t value;
    uint32_t size;
    bool is_signed = false;
    bool is_pre_post_indexed = false;
    
    // Check if this is pre/post indexed addressing
    switch (fields.type) {
        case INST_T32_STR_PRE_POST:
        case INST_T32_STRB_PRE_POST:
        case INST_T32_STRH_PRE_POST:
        case INST_T32_LDR_PRE_POST:
        case INST_T32_LDRB_PRE_POST:
        case INST_T32_LDRH_PRE_POST:
            is_pre_post_indexed = true;
            break;
        default:
            is_pre_post_indexed = false;
            break;
    }
    
    // Calculate address
    if (fields.type == INST_T32_LDR_LIT || fields.type == INST_T32_LDRB_LIT || fields.type == INST_T32_LDRH_LIT) {
        // PC-relative addressing for T32 instructions
        // ARM specification: use (current instruction address + 4) as base
        uint32_t pc = m_registers->get_pc();
        uint32_t base_pc = pc + 4; // PC+4 for T32 instruction
        // Align to word boundary for PC-relative loads
        int32_t signed_offset = fields.negative_offset ? -(int32_t)fields.imm : (int32_t)fields.imm;
        address = (base_pc & 0xFFFFFFFC) + signed_offset;
    } else if (fields.type == INST_T32_LDR_REG || fields.type == INST_T32_STR_REG || 
               fields.type == INST_T32_STRB_REG || fields.type == INST_T32_STRH_REG ||
               fields.type == INST_T32_LDRB_REG || fields.type == INST_T32_LDRH_REG ||
               fields.type == INST_T32_LDRSB_REG || fields.type == INST_T32_LDRSH_REG) {
        // Register offset addressing: [Rn, Rm, LSL #shift]
        uint32_t base_addr = m_registers->read_register(fields.rn);
        uint32_t offset = m_registers->read_register(fields.rm);
        
        // Apply shift if present
        if (fields.shift_amount > 0) {
            offset = offset << fields.shift_amount;
        }
        
        address = base_addr + offset;
    } else if (is_pre_post_indexed) {
        // Pre/post indexed addressing
        uint32_t base_addr = m_registers->read_register(fields.rn);
        int32_t signed_offset = fields.negative_offset ? -(int32_t)fields.imm : (int32_t)fields.imm;
        
        if (fields.pre_indexed) {
            // Pre-indexed: [Rn, #offset]! - adjust address before access
            address = base_addr + signed_offset;
        } else {
            // Post-indexed: [Rn], #offset - use base address, adjust after access
            address = base_addr;
        }
    } else {
        // Register + immediate addressing
        uint32_t base_addr = m_registers->read_register(fields.rn);
        if (fields.negative_offset) {
            address = base_addr - fields.imm;
        } else {
            address = base_addr + fields.imm;
        }
    }
    
    // Determine access size and signedness
    switch (fields.type) {
        case INST_T32_LDR_IMM:
        case INST_T32_LDR_LIT:
        case INST_T32_LDR_REG:
        case INST_T32_LDRT:
        case INST_T32_STR_IMM:
        case INST_T32_STR_REG:
        case INST_T32_LDR_PRE_POST:
        case INST_T32_STR_PRE_POST:
            size = 4; // Word
            break;
        case INST_T32_LDRH_IMM:
        case INST_T32_STRH_IMM:
        case INST_T32_STRH_REG:
        case INST_T32_LDRH_REG:
        case INST_T32_LDRH_PRE_POST:
        case INST_T32_STRH_PRE_POST:
            size = 2; // Halfword
            break;
        case INST_T32_LDRSH_IMM:
        case INST_T32_LDRSH_REG:
            size = 2; // Signed halfword
            is_signed = true;
            break;
        case INST_T32_LDRB_IMM:
        case INST_T32_STRB_IMM:
        case INST_T32_STRB_REG:
        case INST_T32_LDRB_REG:
        case INST_T32_LDRB_PRE_POST:
        case INST_T32_STRB_PRE_POST:
            size = 1; // Byte
            break;
        case INST_T32_LDRSB_IMM:
        case INST_T32_LDRSB_REG:
            size = 1; // Signed byte
            is_signed = true;
            break;
        default:
            LOG_WARNING("Unknown T32 load/store instruction: " + std::to_string(fields.type));
            return false;
    }
    
    // Check alignment for word and halfword accesses; byte (size==1) is always allowed
    if (size > 1 && (address & (size - 1)) != 0) {
        LOG_WARNING("Unaligned access at address " + hex32(address));
        // In real ARM, this could generate an alignment fault
    }
    
    // Execute load or store
    if (fields.load_store_bit) {
        // Load operation
        value = read_memory(address, size, data_bus);
        
        // Zero extend for unsigned loads, sign extend if needed
        if (is_signed) {
            if (size == 1 && (value & 0x80)) {
                value |= 0xFFFFFF00; // Sign extend byte
            } else if (size == 2 && (value & 0x8000)) {
                value |= 0xFFFF0000; // Sign extend halfword
            }
        } else {
            // Zero extend for unsigned loads
            if (size == 1) {
                uint32_t raw_value = value;
                value &= 0xFF; // Zero extend byte
                if (fields.type == INST_T32_LDRB_IMM || fields.type == INST_T32_LDRB_PRE_POST) {
                    LOG_DEBUG("LDRB: raw_value=" + hex32(raw_value) + ", final_value=" + hex32(value) + ", size=" + std::to_string(size));
                }
            } else if (size == 2) {
                value &= 0xFFFF; // Zero extend halfword
            }
        }
        
        // Write to destination register
        m_registers->write_register(fields.rd, value);
        
        // Log the operation
        std::string inst_name;
        switch (fields.type) {
            case INST_T32_LDR_IMM: inst_name = "LDR.W"; break;
            case INST_T32_LDR_LIT: inst_name = "LDR.W"; break;
            case INST_T32_LDR_REG: inst_name = "LDR.W"; break;
            case INST_T32_LDRT: inst_name = "LDRT"; break;
            case INST_T32_LDRB_IMM: inst_name = "LDRB.W"; break;
            case INST_T32_LDRB_REG: inst_name = "LDRB.W"; break;
            case INST_T32_LDRH_IMM: inst_name = "LDRH.W"; break;
            case INST_T32_LDRH_REG: inst_name = "LDRH.W"; break;
            case INST_T32_LDRSB_IMM: inst_name = "LDRSB.W"; break;
            case INST_T32_LDRSB_REG: inst_name = "LDRSB.W"; break;
            case INST_T32_LDRSH_IMM: inst_name = "LDRSH.W"; break;
            case INST_T32_LDRSH_REG: inst_name = "LDRSH.W"; break;
            case INST_T32_LDR_PRE_POST: inst_name = "LDR.W"; break;
            case INST_T32_LDRB_PRE_POST: inst_name = "LDRB.W"; break;
            case INST_T32_LDRH_PRE_POST: inst_name = "LDRH.W"; break;
        }
        
        if (fields.type == INST_T32_LDR_LIT) {
            LOG_DEBUG(inst_name + " " + reg_name(fields.rd) + ", [PC, #" + 
                     std::to_string((int32_t)fields.imm) + "] -> loaded " + hex32(value) + 
                     " from " + hex32(address));
        } else if (fields.type == INST_T32_LDR_REG || fields.type == INST_T32_LDRB_REG || 
                   fields.type == INST_T32_LDRH_REG || fields.type == INST_T32_LDRSB_REG || 
                   fields.type == INST_T32_LDRSH_REG) {
            LOG_DEBUG(inst_name + " " + reg_name(fields.rd) + ", [" + reg_name(fields.rn) + 
                     ", " + reg_name(fields.rm) + "] -> loaded " + hex32(value) + 
                     " from " + hex32(address));
        } else {
            LOG_DEBUG(inst_name + " " + reg_name(fields.rd) + ", [" + reg_name(fields.rn) + 
                     ", #" + std::to_string((int32_t)fields.imm) + "] -> loaded " + hex32(value) + 
                     " from " + hex32(address));
        }
        
    } else {
        // Store operation
        value = m_registers->read_register(fields.rd);
        
        // Mask value based on size
        if (size == 1) {
            value &= 0xFF;
        } else if (size == 2) {
            value &= 0xFFFF;
        }
        
        write_memory(address, value, size, data_bus);
        
        // Log the operation
        std::string inst_name;
        switch (fields.type) {
            case INST_T32_STR_IMM: inst_name = "STR.W"; break;
            case INST_T32_STR_REG: inst_name = "STR.W"; break;
            case INST_T32_STRB_IMM: inst_name = "STRB.W"; break;
            case INST_T32_STRB_REG: inst_name = "STRB.W"; break;
            case INST_T32_STRH_IMM: inst_name = "STRH.W"; break;
            case INST_T32_STRH_REG: inst_name = "STRH.W"; break;
            case INST_T32_STR_PRE_POST: inst_name = "STR.W"; break;
            case INST_T32_STRB_PRE_POST: inst_name = "STRB.W"; break;
            case INST_T32_STRH_PRE_POST: inst_name = "STRH.W"; break;
        }
        
        if (fields.type == INST_T32_STR_REG || fields.type == INST_T32_STRB_REG || fields.type == INST_T32_STRH_REG) {
            LOG_DEBUG(inst_name + " " + reg_name(fields.rd) + ", [" + reg_name(fields.rn) + 
                     ", " + reg_name(fields.rm) + "] -> stored " + hex32(value) + 
                     " to " + hex32(address));
        } else {
            LOG_DEBUG(inst_name + " " + reg_name(fields.rd) + ", [" + reg_name(fields.rn) + 
                     ", #" + std::to_string((int32_t)fields.imm) + "] -> stored " + hex32(value) + 
                     " to " + hex32(address));
        }
    }
    
    // Handle writeback for pre/post indexed addressing
    if (is_pre_post_indexed && fields.writeback) {
        uint32_t base_addr = m_registers->read_register(fields.rn);
        int32_t signed_offset = fields.negative_offset ? -(int32_t)fields.imm : (int32_t)fields.imm;
        uint32_t new_base;
        
        if (fields.pre_indexed) {
            // Pre-indexed: base already updated to address
            new_base = address;
        } else {
            // Post-indexed: update base with offset
            new_base = base_addr + signed_offset;
        }
        m_registers->write_register(fields.rn, new_base);
        LOG_TRACE("[REG] WRITE " + reg_name(fields.rn) + " = " + hex32(new_base) + " (writeback)");
    }
    
    if (fields.rd == 15) {
        return true; // Indicate that PC was changed
    }
    return false; // PC not changed for normal load/store
}

#if HAS_EXCLUSIVE_ACCESS
bool Execute::execute_exclusive_load(const InstructionFields& fields, void* data_bus)
{
    // LDREX/LDREXB/LDREXH - Exclusive Load instructions
    
    uint32_t address = m_registers->read_register(fields.rn);
    uint32_t size;
    
    // Determine access size based on instruction type
    switch (fields.type) {
        case INST_T32_LDREX:
            address += fields.imm;  // Add offset for LDREX
            size = 4;               // Word access
            break;
        case INST_T32_LDREXB:
            size = 1;               // Byte access
            break;
        case INST_T32_LDREXH:
            size = 2;               // Halfword access
            break;
        default:
            LOG_WARNING("Unknown exclusive load instruction: " + std::to_string(fields.type));
            return false;
    }
    
    // Check alignment
    if ((address & (size - 1)) != 0) {
        LOG_WARNING("Unaligned exclusive access at address " + hex32(address));
        // In real ARM, this would generate an alignment fault
    }
    
    // Perform the load
    uint32_t value = read_memory(address, size, data_bus);
    
    // Set exclusive monitor
    m_exclusive_monitor_enabled = true;
    m_exclusive_address = address;
    m_exclusive_size = size;
    
    // Write result to register
    m_registers->write_register(fields.rd, value);
    
    LOG_DEBUG("LDREX: loaded " + hex32(value) + " from address " + hex32(address) + 
             ", exclusive monitor set");
    
    return false;
}

bool Execute::execute_exclusive_store(const InstructionFields& fields, void* data_bus)
{
    // STREX/STREXB/STREXH - Exclusive Store instructions
    
    uint32_t address = m_registers->read_register(fields.rn);
    uint32_t value = m_registers->read_register(fields.rm);  // Read from Rt (data register)
    uint32_t size;
    
    // Determine access size based on instruction type
    switch (fields.type) {
        case INST_T32_STREX:
            address += fields.imm;  // Add offset for STREX
            size = 4;               // Word access
            break;
        case INST_T32_STREXB:
            size = 1;               // Byte access
            break;
        case INST_T32_STREXH:
            size = 2;               // Halfword access
            break;
        default:
            LOG_WARNING("Unknown exclusive store instruction: " + std::to_string(fields.type));
            return false;
    }
    
    // Check alignment
    if ((address & (size - 1)) != 0) {
        LOG_WARNING("Unaligned exclusive access at address " + hex32(address));
    }
    
    uint32_t status = 0;  // Success by default
    
    // Check if exclusive monitor matches
    if (!m_exclusive_monitor_enabled || 
        m_exclusive_address != address || 
        m_exclusive_size != size) {
        // Exclusive monitor mismatch - store fails
        status = 1;
        LOG_DEBUG("STREX: failed (monitor mismatch), address=" + hex32(address));
    } else {
        // Exclusive monitor matches - perform store
        write_memory(address, value, size, data_bus);
        status = 0;
        LOG_DEBUG("STREX: stored " + hex32(value) + " to address " + hex32(address) + ", success");
    }
    
    // Clear exclusive monitor after any STREX attempt
    m_exclusive_monitor_enabled = false;
    m_exclusive_address = 0;
    m_exclusive_size = 0;
    
    // Write status to result register (Rd for STREX)
    m_registers->write_register(fields.rd, status);
    
    return false;
}
#endif

#if HAS_HARDWARE_DIVIDE
bool Execute::execute_divide(const InstructionFields& fields)
{
    // UDIV/SDIV - Hardware divide instructions
    
    uint32_t dividend = m_registers->read_register(fields.rn);
    uint32_t divisor = m_registers->read_register(fields.rm);
    uint32_t result;
    
    // Check for division by zero
    if (divisor == 0) {
        // ARM behavior: result is 0 when dividing by zero
        result = 0;
        LOG_WARNING("Division by zero, result set to 0");
    } else {
        if (fields.type == INST_T32_UDIV) {
            // Unsigned divide
            result = dividend / divisor;
            LOG_DEBUG("UDIV: " + std::to_string(dividend) + " / " + std::to_string(divisor) + " = " + std::to_string(result));
        } else {
            // Signed divide
            int32_t signed_dividend = (int32_t)dividend;
            int32_t signed_divisor = (int32_t)divisor;
            int32_t signed_result = signed_dividend / signed_divisor;
            result = (uint32_t)signed_result;
            LOG_DEBUG("SDIV: " + std::to_string(signed_dividend) + " / " + std::to_string(signed_divisor) + " = " + std::to_string(signed_result));
        }
    }
    
    m_registers->write_register(fields.rd, result);
    return false;
}

bool Execute::execute_mul(const InstructionFields& fields)
{
    // MUL - Multiply: Rd = Rn * Rm
    
    uint32_t multiplicand = m_registers->read_register(fields.rn);
    uint32_t multiplier = m_registers->read_register(fields.rm);
    
    // Perform multiply
    uint32_t result = multiplicand * multiplier;
    
    m_registers->write_register(fields.rd, result);
    
    LOG_DEBUG("MUL " + std::to_string(fields.rd) + ", " + reg_name(fields.rn) + 
             ", " + reg_name(fields.rm) + 
             " -> " + std::to_string(multiplicand) + 
             " * " + std::to_string(multiplier) + " = " + std::to_string(result));
    
    return false;
}

bool Execute::execute_mla(const InstructionFields& fields)
{
    // MLA - Multiply and Accumulate: Rd = Ra + (Rn * Rm)
    
    uint32_t multiplicand = m_registers->read_register(fields.rn);
    uint32_t multiplier = m_registers->read_register(fields.rm);
    uint32_t addend = m_registers->read_register(fields.rs);  // Ra
    
    // Perform multiply and accumulate
    uint32_t product = multiplicand * multiplier;
    uint32_t result = addend + product;
    
    m_registers->write_register(fields.rd, result);
    
    LOG_DEBUG("MLA " + std::to_string(fields.rd) + ", " + reg_name(fields.rn) + 
             ", " + reg_name(fields.rm) + ", " + reg_name(fields.rs) + 
             " -> " + std::to_string(addend) + " + (" + std::to_string(multiplicand) + 
             " * " + std::to_string(multiplier) + ") = " + std::to_string(result));
    
    return false;
}

bool Execute::execute_mls(const InstructionFields& fields)
{
    // MLS - Multiply and Subtract: Rd = Ra - (Rn * Rm)
    
    uint32_t multiplicand = m_registers->read_register(fields.rn);
    uint32_t multiplier = m_registers->read_register(fields.rm);
    uint32_t addend = m_registers->read_register(fields.rs);  // Ra
    
    // Perform multiply and subtract
    uint32_t product = multiplicand * multiplier;
    uint32_t result = addend - product;
    
    m_registers->write_register(fields.rd, result);
    
    LOG_DEBUG("MLS " + std::to_string(fields.rd) + ", " + reg_name(fields.rn) + 
             ", " + reg_name(fields.rm) + ", " + std::to_string(fields.rs) + 
             " -> " + std::to_string(addend) + " - (" + std::to_string(multiplicand) + 
             " * " + std::to_string(multiplier) + ") = " + std::to_string(result));
    
    return false;
}

bool Execute::execute_long_multiply(const InstructionFields& fields)
{
    // Long multiply operations: UMULL, SMULL, UMLAL, SMLAL
    
    uint32_t operand1 = m_registers->read_register(fields.rn);
    uint32_t operand2 = m_registers->read_register(fields.rm);
    uint64_t result64;
    
    switch (fields.type) {
        case INST_T32_UMULL: {
            // UMULL - Unsigned Multiply Long: RdHi:RdLo = Rn * Rm
            result64 = static_cast<uint64_t>(operand1) * static_cast<uint64_t>(operand2);
            break;
        }
        case INST_T32_SMULL: {
            // SMULL - Signed Multiply Long: RdHi:RdLo = Rn * Rm (signed)
            result64 = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(operand1)) * 
                                           static_cast<int64_t>(static_cast<int32_t>(operand2)));
            break;
        }
        case INST_T32_UMLAL: {
            // UMLAL - Unsigned Multiply Accumulate Long: RdHi:RdLo += Rn * Rm
            uint32_t rdlo_current = m_registers->read_register(fields.rd);
            uint32_t rdhi_current = m_registers->read_register(fields.rs);
            uint64_t current_val = (static_cast<uint64_t>(rdhi_current) << 32) | rdlo_current;
            uint64_t product = static_cast<uint64_t>(operand1) * static_cast<uint64_t>(operand2);
            result64 = current_val + product;
            break;
        }
        case INST_T32_SMLAL: {
            // SMLAL - Signed Multiply Accumulate Long: RdHi:RdLo += Rn * Rm (signed)
            uint32_t rdlo_current = m_registers->read_register(fields.rd);
            uint32_t rdhi_current = m_registers->read_register(fields.rs);
            int64_t current_val = (static_cast<int64_t>(static_cast<int32_t>(rdhi_current)) << 32) | rdlo_current;
            int64_t product = static_cast<int64_t>(static_cast<int32_t>(operand1)) * 
                             static_cast<int64_t>(static_cast<int32_t>(operand2));
            result64 = static_cast<uint64_t>(current_val + product);
            break;
        }
        default:
            LOG_ERROR("Unknown long multiply instruction: " + std::to_string(fields.type));
            return false;
    }
    
    // Write results to RdLo and RdHi
    uint32_t rdlo = static_cast<uint32_t>(result64 & 0xFFFFFFFF);
    uint32_t rdhi = static_cast<uint32_t>(result64 >> 32);
    
    m_registers->write_register(fields.rd, rdlo);  // RdLo
    m_registers->write_register(fields.rs, rdhi);  // RdHi
    
    LOG_DEBUG("Long multiply: " + std::to_string(fields.rd) + ", " + std::to_string(fields.rs) + 
             ", " + reg_name(fields.rn) + ", " + reg_name(fields.rm) + 
             " -> " + std::to_string(operand1) + " * " + std::to_string(operand2) + 
             " = " + hex32(rdhi) + " " +hex32(rdlo));
    
    return false;
}
#endif

#if HAS_BITFIELD_INSTRUCTIONS  
bool Execute::execute_bitfield(const InstructionFields& fields)
{
    // Bitfield operations: BFI, BFC, UBFX, SBFX
    
    uint32_t lsb, width;
    
    if (fields.type == INST_T32_UBFX || fields.type == INST_T32_SBFX) {
        // For UBFX/SBFX: width is stored in upper bits, lsb in lower bits
        width = fields.imm & 0x1F;        // bits [4:0]
        lsb = (fields.imm >> 8) & 0x1F; // bits [12:8]
    } else {
        // For BFI/BFC: calculate width from msb and lsb
        width = fields.imm & 0x1F;        // bits [4:0]
        lsb = (fields.imm >> 8) & 0x1F; // bits [12:8]
    }
    
    switch (fields.type) {
        case INST_T32_BFI: {
            // BFI - Bit Field Insert
            uint32_t src = m_registers->read_register(fields.rn);
            uint32_t dst = m_registers->read_register(fields.rd);
            uint32_t mask = ((1U << width) - 1) << lsb;
            uint32_t result = (dst & ~mask) | ((src << lsb) & mask);
            m_registers->write_register(fields.rd, result);
            LOG_DEBUG("BFI: inserted " + std::to_string(width) + " bits at position " + std::to_string(lsb));
            break;
        }
        
        case INST_T32_BFC: {
            // BFC - Bit Field Clear
            uint32_t dst = m_registers->read_register(fields.rd);
            uint32_t mask = ((1U << width) - 1) << lsb;
            uint32_t result = dst & ~mask;
            m_registers->write_register(fields.rd, result);
            LOG_DEBUG("BFC: cleared " + std::to_string(width) + " bits at position " + std::to_string(lsb));
            break;
        }
        
        case INST_T32_UBFX: {
            // UBFX - Unsigned Bit Field Extract
            uint32_t src = m_registers->read_register(fields.rn);
            uint32_t result = (src >> lsb) & ((1U << width) - 1);
            m_registers->write_register(fields.rd, result);
            LOG_DEBUG("UBFX: extracted " + std::to_string(width) + " bits from position " + std::to_string(lsb));
            break;
        }
        
        case INST_T32_SBFX: {
            // SBFX - Signed Bit Field Extract
            uint32_t src = m_registers->read_register(fields.rn);
            uint32_t extracted = (src >> lsb) & ((1U << width) - 1);
            // Sign extend if MSB of extracted field is set
            if (extracted & (1U << (width - 1))) {
                extracted |= (~0U << width);
            }
            m_registers->write_register(fields.rd, extracted);
            LOG_DEBUG("SBFX: extracted " + std::to_string(width) + " bits from position " + std::to_string(lsb));
            break;
        }
        
        default:
            LOG_WARNING("Unknown bitfield instruction: " + std::to_string(fields.type));
            break;
    }
    
    return false;
}
#endif

#if HAS_SATURATING_ARITHMETIC
bool Execute::execute_saturate(const InstructionFields& fields)
{
    // Saturating arithmetic: SSAT, USAT
    
    uint32_t src = m_registers->read_register(fields.rn);
    uint32_t sat_imm = (fields.imm >> 16) & 0x1F;   // Saturation limit
    uint32_t shift_type = (fields.imm >> 8) & 0x1;  // Shift type
    uint32_t shift = fields.imm & 0x7F;             // Shift amount
    
    // Apply shift first (if any)
    uint32_t shifted_value = shift_type ? (src >> shift) : (src << shift); // 1: right shift, 0: left shift
    int32_t signed_value = (int32_t)shifted_value;
    bool saturated = false;
    uint32_t result;

    if (fields.type == INST_T32_SSAT) {
        // SSAT - Signed Saturate
        int32_t max_val = (1 << (sat_imm - 1)) - 1;
        int32_t min_val = -(1 << (sat_imm - 1));

        if (signed_value > max_val) {
            result = max_val;
            saturated = true;
        } else if (signed_value < min_val) {
            result = min_val;
            saturated = true;
        } else {
            result = signed_value;
        }
        
        LOG_DEBUG("SSAT: value=" + std::to_string(signed_value) + ", [" + std::to_string(min_val) + ", " + std::to_string(max_val) + 
                 "], result=" + std::to_string((int32_t)result) + (saturated ? " (saturated)" : ""));
    } else {
        // USAT - Unsigned Saturate
        int32_t max_val = (1U << sat_imm) - 1;
        int32_t min_val = 0;

        if (signed_value > max_val) {
            result = max_val;
            saturated = true;
        } else if (signed_value < min_val) {
            result = min_val;
            saturated = true;
        } else {
            result = signed_value;
        }
        
        LOG_DEBUG("USAT: value=" + std::to_string(shifted_value) + ", [" + std::to_string(min_val) + ", " + std::to_string(max_val) + 
                 "], result=" + std::to_string(result) + (saturated ? " (saturated)" : ""));
    }

    m_registers->write_register(fields.rd, result);
    
    // Set Q flag in APSR if saturation occurred
    if (saturated) {
        uint32_t apsr = m_registers->get_apsr();
        apsr |= (1 << 27); // Set Q flag (bit 27)
        m_registers->set_apsr(apsr);
    }
    
    return false;
}
#endif

#if HAS_BIT_MANIPULATION
bool Execute::execute_bit_manipulation(const InstructionFields& fields)
{
    uint32_t src_value = m_registers->read_register(fields.rm);
    uint32_t result = 0;
    
    switch (fields.type) {
        case INST_T32_CLZ: {
            // Count Leading Zeros
            result = 0;
            if (src_value == 0) {
                result = 32;
            } else {
                uint32_t mask = 0x80000000;
                while ((src_value & mask) == 0) {
                    result++;
                    mask >>= 1;
                }
            }
            LOG_DEBUG("CLZ " + std::to_string(fields.rd) + ", " + reg_name(fields.rm) + 
                     " -> " + std::to_string(result) + " leading zeros in " + hex32(src_value));
            break;
        }
        
        case INST_T32_RBIT: {
            // Reverse Bits
            result = 0;
            for (int i = 0; i < 32; i++) {
                if (src_value & (1U << i)) {
                    result |= (1U << (31 - i));
                }
            }
            LOG_DEBUG("RBIT " + std::to_string(fields.rd) + ", " + reg_name(fields.rm) + 
                     " -> " + hex32(result) + " (bits reversed from " + hex32(src_value) + ")");
            break;
        }
        
        case INST_T32_REV: {
            // Byte-Reverse Word (swap bytes: ABCD -> DCBA)
            result = ((src_value & 0xFF000000) >> 24) |
                    ((src_value & 0x00FF0000) >> 8) |
                    ((src_value & 0x0000FF00) << 8) |
                    ((src_value & 0x000000FF) << 24);
            LOG_DEBUG("REV " + std::to_string(fields.rd) + ", " + reg_name(fields.rm) + 
                     " -> " + hex32(result) + " (bytes reversed from " + hex32(src_value) + ")");
            break;
        }
        
        case INST_T32_REV16: {
            // Byte-Reverse Packed Halfword (swap bytes in each halfword: ABCD -> BADC)
            result = ((src_value & 0xFF000000) >> 8) |
                    ((src_value & 0x00FF0000) << 8) |
                    ((src_value & 0x0000FF00) >> 8) |
                    ((src_value & 0x000000FF) << 8);
            LOG_DEBUG("REV16 " + std::to_string(fields.rd) + ", " + reg_name(fields.rm) + 
                     " -> " + hex32(result) + " (halfword bytes reversed from " + hex32(src_value) + ")");
            break;
        }
        
        case INST_T32_REVSH: {
            // Byte-Reverse Signed Halfword (reverse bytes in lower halfword and sign extend)
            uint16_t lower_half = src_value & 0xFFFF;
            uint16_t reversed = ((lower_half & 0xFF00) >> 8) | ((lower_half & 0x00FF) << 8);
            result = (int32_t)(int16_t)reversed; // Sign extend to 32 bits
            LOG_DEBUG("REVSH " + std::to_string(fields.rd) + ", " + reg_name(fields.rm) + 
                     " -> " + hex32(result) + " (signed halfword bytes reversed from " + hex32(src_value) + ")");
            break;
        }
        
        default:
            LOG_ERROR("Unknown bit manipulation instruction type: " + std::to_string(fields.type));
            return false;
    }
    
    m_registers->write_register(fields.rd, result);
    return false;
}
#endif

bool Execute::execute_t32_dual_load_store(const InstructionFields& fields, void* data_bus)
{
    auto* bus = static_cast<tlm_utils::simple_initiator_socket<CPU>*>(data_bus);
    
    // Calculate base address
    uint32_t base_addr;
    if (fields.rn == 15) { // PC-relative
        // Align PC to word boundary for literal-style addressing
        base_addr = (m_registers->get_pc() & ~3u) + 4; // PC already advanced by 4; keep alignment consistent
    } else {
        base_addr = m_registers->read_register(fields.rn);
    }
    int32_t signed_offset = fields.negative_offset ? -(int32_t)fields.imm : (int32_t)fields.imm;
    uint32_t address;

    if (fields.pre_indexed) {
        // Pre-indexed: [Rn, #offset]! - adjust address before access
        address = base_addr + signed_offset;
    } else {
        // Post-indexed: [Rn], #offset - use base address, adjust after access
        address = base_addr;
    }

    // Check alignment (dual operations must be 4-byte aligned)
    if (address & 0x3) {
        LOG_ERROR("Unaligned dual load/store at address " + hex32(address));
        // Generate alignment fault or use unaligned access
    }

    if (fields.type == INST_T32_LDRD) {
        // Load dual registers
        uint32_t data1 = read_memory(address, 4, bus);
        uint32_t data2 = read_memory(address + 4, 4, bus);
        
        m_registers->write_register(fields.rd, data1);
        m_registers->write_register(fields.rm, data2);
        
        LOG_DEBUG("LDRD " + reg_name(fields.rd) + ", " + reg_name(fields.rm) + 
                 ", [" + (fields.rn == 15 ? "pc" : "" + reg_name(fields.rn)) + 
                 ", #" + std::to_string(fields.imm) + "] -> loaded " + hex32(data1) + 
                 ", " + hex32(data2) + " from " + hex32(address));
    } else {
        // Store dual registers
        uint32_t data1 = m_registers->read_register(fields.rd);
        uint32_t data2 = m_registers->read_register(fields.rm);
        
        write_memory(address, data1, 4, bus);
        write_memory(address + 4, data2, 4, bus);
        
        LOG_DEBUG("STRD " + reg_name(fields.rd) + ", " + reg_name(fields.rm) + 
                 ", [" + (fields.rn == 15 ? "pc" : "" + reg_name(fields.rn)) + 
                 ", #" + std::to_string(fields.imm) + "] -> stored " + hex32(data1) + 
                 ", " + hex32(data2) + " to " + hex32(address));
    }

    // Handle writeback for pre/post indexed addressing
    if (fields.pre_indexed && fields.writeback) {
        uint32_t base_addr = m_registers->read_register(fields.rn);
        int32_t signed_offset = fields.negative_offset ? -(int32_t)fields.imm : (int32_t)fields.imm;
        uint32_t new_base;
        
        if (fields.pre_indexed) {
            // Pre-indexed: base already updated to address
            new_base = address;
        } else {
            // Post-indexed: update base with offset
            new_base = base_addr + signed_offset;
        }
        m_registers->write_register(fields.rn, new_base);
        LOG_TRACE("[REG] WRITE " + reg_name(fields.rn) + " = " + hex32(new_base) + " (writeback)");
    }

    return false;
}

bool Execute::execute_t32_multiple_load_store(const InstructionFields& fields, void* data_bus)
{
    auto* bus = static_cast<tlm_utils::simple_initiator_socket<CPU>*>(data_bus);
    
    uint32_t address = m_registers->read_register(fields.rn);
    uint16_t reg_list = fields.reg_list;
    bool is_load = fields.load_store_bit;
    
    // Count number of registers in list for address calculation
    int reg_count = 0;
    for (int i = 0; i < 16; i++) {
        if (reg_list & (1 << i)) {
            reg_count++;
        }
    }
    
    // Special case: When LDMDB SP! is used in function epilogues, it often should behave like LDMIA SP!
    // This is a common pattern where the intention is to pop from the stack (increment after)
    bool treat_ldmdb_sp_as_ldmia = (fields.type == INST_T32_LDMDB && fields.rn == 13 && (reg_list & 0x8000)); // PC in list
    
    // For STMDB/LDMDB (except special case), adjust starting address
    if ((fields.type == INST_T32_STMDB || fields.type == INST_T32_LDMDB) && !treat_ldmdb_sp_as_ldmia) {
        address -= reg_count * 4;
    }
    
    uint32_t current_addr = address;
    bool pc_loaded = false;
    
    if (is_load) {
        // Multiple load
        for (int i = 0; i < 16; i++) {
            if (reg_list & (1 << i)) {
                uint32_t data = read_memory(current_addr, 4, bus);
                
                // Special handling for loading PC
                if (i == 15) {
                    m_registers->set_pc(data & ~1); // Clear LSB for Thumb mode
                    // Clear IT state on control-flow change
                    m_registers->clear_it_state();
                    pc_loaded = true;
                } else {
                    m_registers->write_register(i, data);
                }
                current_addr += 4;
            }
        }
        /*
        LOG_DEBUG("LDMIA/LDMDB " + reg_name(fields.rn) + 
                 "!, " + format_reg_list(reg_list) + " from " + hex32(address) +
                 (treat_ldmdb_sp_as_ldmia ? " (LDMDB SP! treated as LDMIA)" : ""));
        */
    } else {
        // Multiple store
        for (int i = 0; i < 16; i++) {
            if (reg_list & (1 << i)) {
                uint32_t data = m_registers->read_register(i);
                write_memory(current_addr, data, 4, bus);
                current_addr += 4;
            }
        }
        /*
        LOG_DEBUG("STMIA/STMDB " + reg_name(fields.rn) + 
                 "!, " + format_reg_list(reg_list) + " to " + hex32(address));
        */
    }
    
    // Update base register (write-back) - always do this regardless of PC loading
    if (fields.type == INST_T32_LDMIA || fields.type == INST_T32_STMIA || treat_ldmdb_sp_as_ldmia) {
        // Increment after (or treat LDMDB SP! with PC as LDMIA)
        m_registers->write_register(fields.rn, address + reg_count * 4);
    } else {
        // Decrement before - base register already points to correct final address
        m_registers->write_register(fields.rn, address);
    }
    
    // Return true if PC was changed
    if (pc_loaded) {
        return true;
    }
    
    return false;
}

// Helper function to apply ARM shift operations
uint32_t Execute::apply_shift(uint32_t value, uint8_t shift_type, uint8_t shift_amount)
{
    if (shift_amount == 0) {
        return value;
    }
    
    switch (shift_type) {
        case 0x0: // LSL (Logical Shift Left)
            return (shift_amount >= 32) ? 0 : (value << shift_amount);
            
        case 0x1: // LSR (Logical Shift Right)
            return (shift_amount >= 32) ? 0 : (value >> shift_amount);
            
        case 0x2: // ASR (Arithmetic Shift Right)
            if (shift_amount >= 32) {
                return (value & 0x80000000) ? 0xFFFFFFFF : 0;
            }
            return ((int32_t)value) >> shift_amount;
            
        case 0x3: // ROR (Rotate Right)
            shift_amount &= 0x1F; // Only use bottom 5 bits for rotate
            if (shift_amount == 0) {
                return value;
            }
            return (value >> shift_amount) | (value << (32 - shift_amount));
            
        default:
            return value;
    }
}

#endif // SUPPORTS_ARMV7_M