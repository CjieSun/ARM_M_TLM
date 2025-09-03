#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <systemc>
#include <cstdint>
#include "ARM_CortexM_Config.h"

using namespace sc_core;

// ARMv6-M Thumb instruction types (granular, per A5 encoding)
// Prefix T16_ for 16-bit Thumb, T32_ for 32-bit Thumb encodings
enum InstructionType {
    INST_UNKNOWN = 0,

    // --- T16: Shift (immediate) --- (Format 1)
    INST_T16_LSL_IMM,           // LSL Rd, Rm, #imm5
    INST_T16_LSR_IMM,           // LSR Rd, Rm, #imm5
    INST_T16_ASR_IMM,           // ASR Rd, Rm, #imm5

    // --- T16: Add/Sub register/imm3 --- (Format 2)
    INST_T16_ADD_REG,           // ADD Rd, Rn, Rm
    INST_T16_SUB_REG,           // SUB Rd, Rn, Rm
    INST_T16_ADD_IMM3,          // ADD Rd, Rn, #imm3
    INST_T16_SUB_IMM3,          // SUB Rd, Rn, #imm3

    // --- T16: Data processing (immediate) --- (Format 3)
    INST_T16_MOV_IMM,           // MOV Rd, #imm8
    INST_T16_CMP_IMM,           // CMP Rn, #imm8
    INST_T16_ADD_IMM8,          // ADD Rd, #imm8
    INST_T16_SUB_IMM8,          // SUB Rd, #imm8

    // --- T16: Data processing (register) --- (Format 4)
    INST_T16_AND,               // AND Rd, Rm
    INST_T16_EOR,               // EOR Rd, Rm
    INST_T16_LSL_REG,           // LSL Rd, Rm
    INST_T16_LSR_REG,           // LSR Rd, Rm
    INST_T16_ASR_REG,           // ASR Rd, Rm
    INST_T16_ADC,               // ADC Rd, Rm
    INST_T16_SBC,               // SBC Rd, Rm
    INST_T16_ROR,               // ROR Rd, Rm
    INST_T16_TST,               // TST Rn, Rm
    INST_T16_NEG,               // NEG Rd, Rm
    INST_T16_CMP_REG,           // CMP Rn, Rm
    INST_T16_CMN,               // CMN Rn, Rm
    INST_T16_ORR,               // ORR Rd, Rm
    INST_T16_MUL,               // MUL Rd, Rm
    INST_T16_BIC,               // BIC Rd, Rm
    INST_T16_MVN,               // MVN Rd, Rm

    // --- T16: Hi register operations / BX/BLX --- (Format 5)
    INST_T16_ADD_HI,            // ADD Rd(H), Rm
    INST_T16_CMP_HI,            // CMP Rd(H), Rm
    INST_T16_MOV_HI,            // MOV Rd(H), Rm
    INST_T16_BX,                // BX Rm
#if HAS_BLX_REGISTER
    INST_T16_BLX,               // BLX Rm (not in M0/M0+)
#endif

    // --- T16: PC-relative load --- (Format 6)
    INST_T16_LDR_PC,            // LDR Rd, [PC, #imm]

    // --- T16: Load/store (register offset and sign-extended) --- (Format 7)
    INST_T16_STR_REG,           // STR Rt, [Rn, Rm]
    INST_T16_STRH_REG,          // STRH Rt, [Rn, Rm]
    INST_T16_STRB_REG,          // STRB Rt, [Rn, Rm]
    INST_T16_LDRSB_REG,         // LDRSB Rt, [Rn, Rm]
    INST_T16_LDR_REG,           // LDR Rt, [Rn, Rm]
    INST_T16_LDRH_REG,          // LDRH Rt, [Rn, Rm]
    INST_T16_LDRB_REG,          // LDRB Rt, [Rn, Rm]
    INST_T16_LDRSH_REG,         // LDRSH Rt, [Rn, Rm]

    // --- T16: Load/store (immediate) --- (Formats 8,9)
    INST_T16_STR_IMM,           // STR Rt, [Rn, #imm]
    INST_T16_LDR_IMM,           // LDR Rt, [Rn, #imm]
    INST_T16_STRB_IMM,          // STRB Rt, [Rn, #imm]
    INST_T16_LDRB_IMM,          // LDRB Rt, [Rn, #imm]
    INST_T16_STRH_IMM,          // STRH Rt, [Rn, #imm]
    INST_T16_LDRH_IMM,          // LDRH Rt, [Rn, #imm]

    // --- T16: SP-relative load/store --- (Format 11)
    INST_T16_STR_SP,            // STR Rt, [SP, #imm]
    INST_T16_LDR_SP,            // LDR Rt, [SP, #imm]

    // --- T16: Load address --- (Format 12)
    INST_T16_ADD_PC,            // ADD Rd, PC, #imm
    INST_T16_ADD_SP,            // ADD Rd, SP, #imm

    // --- T16: Add/sub SP immediate --- (Format 13)
    INST_T16_ADD_SP_IMM7,       // ADD SP, #imm7
    INST_T16_SUB_SP_IMM7,       // SUB SP, #imm7

    // --- T16: Push/Pop --- (Format 14)
    INST_T16_PUSH,              // PUSH {reglist, LR}
    INST_T16_POP,               // POP {reglist, PC}
    
    // --- T16: Extend instructions --- (Format 12 subset)
    INST_T16_EXTEND,            // SXTH, SXTB, UXTH, UXTB
    
    // --- T16: Change Processor State --- (Format 12 subset)
    INST_T16_CPS,               // CPSIE/CPSID

    // --- T16: Multiple load/store --- (Format 15)
    INST_T16_STMIA,             // STMIA Rn!, reglist
    INST_T16_LDMIA,             // LDMIA Rn!, reglist

    // --- T16: Branch/exception --- (Formats 16-19)
    INST_T16_B_COND,            // B<cond> label
    INST_T16_SVC,               // SVC #imm8
    INST_T16_B,                 // B label (unconditional)
    INST_T16_BKPT,              // BKPT #imm8
    INST_T16_HINT,              // NOP/WFI/WFE/YIELD/SEV

#if HAS_T32_BL
    // --- T32: (ARMv6-M supports only BL) ---
    INST_T32_BL,                // BL label
#endif
    
#if HAS_MEMORY_BARRIERS
    // --- T32: Memory barriers (ARMv6-M+) ---
    INST_T32_DSB,               // DSB #option
    INST_T32_DMB,               // DMB #option  
    INST_T32_ISB,               // ISB #option
#endif
    
#if HAS_SYSTEM_REGISTERS
    // --- T32: System register access ---
    INST_T32_MSR,               // MSR spec_reg, Rn
    INST_T32_MRS,               // MRS Rd, spec_reg
#endif

#if SUPPORTS_ARMV7_M
    // --- T32: ARMv7-M Extended Instructions ---
    INST_T32_CBZ,               // CBZ Rn, label
    INST_T32_CBNZ,              // CBNZ Rn, label
    INST_T32_IT,                // IT (If-Then)
#endif

#if HAS_HARDWARE_DIVIDE
    // --- T32: Hardware Divide (ARMv7-M+) ---
    INST_T32_UDIV,              // UDIV Rd, Rn, Rm
    INST_T32_SDIV,              // SDIV Rd, Rn, Rm
#endif

#if HAS_BITFIELD_INSTRUCTIONS
    // --- T32: Bitfield Instructions (ARMv7-M+) ---
    INST_T32_BFI,               // BFI Rd, Rn, #lsb, #width
    INST_T32_BFC,               // BFC Rd, #lsb, #width
    INST_T32_UBFX,              // UBFX Rd, Rn, #lsb, #width
    INST_T32_SBFX,              // SBFX Rd, Rn, #lsb, #width
#endif

#if HAS_SATURATING_ARITHMETIC
    // --- T32: Saturating Arithmetic (ARMv7-M+) ---
    INST_T32_SSAT,              // SSAT Rd, #imm, Rn
    INST_T32_USAT,              // USAT Rd, #imm, Rn
#endif

#if SUPPORTS_ARMV7E_M && HAS_DSP_EXTENSIONS
    // --- T32: DSP Extensions (ARMv7E-M) ---
    INST_T32_SMUL,              // SMUL variants
    INST_T32_SMLAL,             // SMLAL variants
    INST_T32_QADD,              // QADD Rd, Rn, Rm
    INST_T32_QSUB,              // QSUB Rd, Rn, Rm
#endif

#if SUPPORTS_ARMV8_M && HAS_SECURITY_EXTENSIONS
    // --- T32: ARMv8-M Security Extensions ---
    INST_T32_SG,                // SG (Secure Gateway)
    INST_T32_BLXNS,             // BLXNS (Branch and Link, Non-Secure)
    INST_T32_BXNS,              // BXNS (Branch, Non-Secure)
#endif
};

// Instruction fields structure
struct InstructionFields {
    uint32_t opcode;     // Raw instruction (16-bit or 32-bit)
    uint8_t rd;          // Destination register
    uint8_t rn;          // First operand register  
    uint8_t rm;          // Second operand register
    uint8_t rs;          // Shift register / third operand
    uint32_t imm;        // Immediate value (extended for 32-bit instructions)
    uint8_t cond;        // Condition code
    bool s_bit;          // Set flags bit
    uint8_t shift_type;  // Shift type (LSL, LSR, ASR, ROR)
    uint8_t shift_amount; // Shift amount
    uint8_t alu_op;      // ALU operation code
    bool h1, h2;         // Hi register flags
    uint16_t reg_list;   // Register list for multiple load/store
    bool load_store_bit; // Load (1) or Store (0)
    uint8_t byte_word;   // Byte (1) or Word (0)
    bool is_32bit;       // True if 32-bit instruction
    InstructionType type;
};

class Instruction : public sc_module
{
public:
    // Constructor
    SC_HAS_PROCESS(Instruction);
    Instruction(sc_module_name name);
    
    // Decode instruction (handles both 16-bit and 32-bit)
    InstructionFields decode(uint32_t instruction, bool is_32bit = false);
    
    // Check if instruction is 32-bit (Thumb-2)
    bool is_32bit_instruction(uint32_t instruction);
    
private:
    // Core Thumb instruction decoding functions
    InstructionFields decode_thumb16_instruction(uint16_t instruction);
    InstructionFields decode_thumb32_instruction(uint32_t instruction);
};

#endif // INSTRUCTION_H