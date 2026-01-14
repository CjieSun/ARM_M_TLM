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
    INST_UNDEFINED,             // Undefined instruction

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
    
    // --- T16: Reverse instructions --- (Format 12 subset)
    INST_T16_REV,               // REV Rd, Rm
    INST_T16_REV16,             // REV16 Rd, Rm
    INST_T16_REVSH,             // REVSH Rd, Rm
    
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

    INST_T16_NOP,               // NOP
    INST_T16_WFI,               // WFI (Wait for Interrupt)
    INST_T16_WFE,               // WFE (Wait for Event)  
    INST_T16_SEV,               // SEV (Send Event)
    INST_T16_YIELD,             // YIELD

#if SUPPORTS_ARMV7_M
#if HAS_BLX_REGISTER
    INST_T16_BLX,               // BLX Rm (not in M0/M0+)
#endif

    // --- T16: ARMv7-M Extended 16-bit Instructions ---
    INST_T16_CBZ,               // CBZ Rn, label
    INST_T16_CBNZ,              // CBNZ Rn, label
    INST_T16_IT,                // IT (If-Then)
#endif

#if HAS_T32_BL
    // --- T32: Branch Instructions ---
    INST_T32_B,                 // B label (unconditional)
    INST_T32_B_COND,            // B<cond> label (conditional)
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
    INST_T32_CBZ,               // CBZ Rn, label (also 32-bit variant)
    INST_T32_CBNZ,              // CBNZ Rn, label (also 32-bit variant)
    INST_T32_IT,                // IT (If-Then) (also 32-bit variant)
    INST_T32_TBB,               // TBB [Rn, Rm] (Table Branch Byte)
    INST_T32_TBH,               // TBH [Rn, Rm, LSL #1] (Table Branch Halfword)
    INST_T32_CLREX,             // CLREX (Clear Exclusive)
    
    // --- T32: Data Processing Instructions ---
    INST_T32_MOV_IMM,           // MOV.W Rd, #imm
    INST_T32_MOVS_IMM,          // MOVS.W Rd, #imm
    INST_T32_MVN_IMM,           // MVN.W Rd, #imm
    INST_T32_ADD_IMM,           // ADD.W Rd, Rn, #imm
    INST_T32_SUB_IMM,           // SUB.W Rd, Rn, #imm
    INST_T32_ADDW,              // ADDW Rd, Rn, #imm12 (12-bit immediate)
    INST_T32_SUBW,              // SUBW Rd, Rn, #imm12 (12-bit immediate)
    INST_T32_MOVW,              // MOVW Rd, #imm16 (16-bit immediate)
    INST_T32_MOVT,              // MOVT Rd, #imm16 (16-bit immediate)
    INST_T32_ADR,               // ADR Rd, label (PC-relative address)
    INST_T32_ADC_IMM,           // ADC.W Rd, Rn, #imm
    INST_T32_SBC_IMM,           // SBC.W Rd, Rn, #imm
    INST_T32_RSB_IMM,           // RSB.W Rd, Rn, #imm
    INST_T32_AND_IMM,           // AND.W Rd, Rn, #imm
    INST_T32_ORR_IMM,           // ORR.W Rd, Rn, #imm
    INST_T32_EOR_IMM,           // EOR.W Rd, Rn, #imm
    INST_T32_BIC_IMM,           // BIC.W Rd, Rn, #imm
    INST_T32_ORN_IMM,           // ORN.W Rd, Rn, #imm
    INST_T32_CMP_IMM,           // CMP.W Rn, #imm
    INST_T32_CMN_IMM,           // CMN.W Rn, #imm
    INST_T32_TST_IMM,           // TST.W Rn, #imm
    INST_T32_TEQ_IMM,           // TEQ.W Rn, #imm
    
    // --- T32: Data Processing Instructions (Register operands) ---
    INST_T32_AND_REG,           // AND.W Rd, Rn, Rm{, shift}
    INST_T32_ANDS_REG,          // ANDS.W Rd, Rn, Rm{, shift}
    INST_T32_ORR_REG,           // ORR.W Rd, Rn, Rm{, shift}
    INST_T32_ORRS_REG,          // ORRS.W Rd, Rn, Rm{, shift}
    INST_T32_ORN_REG,           // ORN.W Rd, Rn, Rm{, shift}
    INST_T32_ORNS_REG,          // ORNS.W Rd, Rn, Rm{, shift}
    INST_T32_EOR_REG,           // EOR.W Rd, Rn, Rm{, shift}
    INST_T32_EORS_REG,          // EORS.W Rd, Rn, Rm{, shift}
    INST_T32_BIC_REG,           // BIC.W Rd, Rn, Rm{, shift}
    INST_T32_BICS_REG,          // BICS.W Rd, Rn, Rm{, shift}
    INST_T32_ADD_REG,           // ADD.W Rd, Rn, Rm{, shift}
    INST_T32_ADDS_REG,          // ADDS.W Rd, Rn, Rm{, shift}
    INST_T32_SUB_REG,           // SUB.W Rd, Rn, Rm{, shift}
    INST_T32_SUBS_REG,          // SUBS.W Rd, Rn, Rm{, shift}
    INST_T32_ADC_REG,           // ADC.W Rd, Rn, Rm{, shift}
    INST_T32_ADCS_REG,          // ADCS.W Rd, Rn, Rm{, shift}
    INST_T32_SBC_REG,           // SBC.W Rd, Rn, Rm{, shift}
    INST_T32_SBCS_REG,          // SBCS.W Rd, Rn, Rm{, shift}
    INST_T32_RSB_REG,           // RSB.W Rd, Rn, Rm{, shift}
    INST_T32_RSBS_REG,          // RSBS.W Rd, Rn, Rm{, shift}
    INST_T32_MOV_REG,           // MOV.W Rd, Rm{, shift}
    INST_T32_MOVS_REG,          // MOVS.W Rd, Rm{, shift}
    INST_T32_MVN_REG,           // MVN.W Rd, Rm{, shift}
    INST_T32_MVNS_REG,          // MVNS.W Rd, Rm{, shift}
    INST_T32_PKH_REG,           // PKH Rd, Rn, Rm{, shift}
    INST_T32_TST_REG,           // TST.W Rn, Rm{, shift}
    INST_T32_TEQ_REG,           // TEQ.W Rn, Rm{, shift}
    INST_T32_CMP_REG,           // CMP.W Rn, Rm{, shift}
    INST_T32_CMN_REG,           // CMN.W Rn, Rm{, shift}
    
    // --- T32: Shift Instructions (register) ---
    INST_T32_LSL_REG,           // LSL.W Rd, Rm, Rs
    INST_T32_LSLS_REG,          // LSLS.W Rd, Rm, Rs  
    INST_T32_LSR_REG,           // LSR.W Rd, Rm, Rs
    INST_T32_LSRS_REG,          // LSRS.W Rd, Rm, Rs
    INST_T32_ASR_REG,           // ASR.W Rd, Rm, Rs
    INST_T32_ASRS_REG,          // ASRS.W Rd, Rm, Rs
    INST_T32_ROR_REG,           // ROR.W Rd, Rm, Rs
    INST_T32_RORS_REG,          // RORS.W Rd, Rm, Rs

    // --- T32: Shift Instructions (immediate) ---
    INST_T32_LSL_IMM,           // LSL.W Rd, Rm, #imm
    INST_T32_LSR_IMM,           // LSR.W Rd, Rm, #imm
    INST_T32_ASR_IMM,           // ASR.W Rd, Rm, #imm
    INST_T32_ROR_IMM,           // ROR.W Rd, Rm, #imm
    
    // --- T32: Load/Store Instructions ---
    INST_T32_LDR_IMM,           // LDR.W Rt, [Rn, #imm]
    INST_T32_LDRB_IMM,          // LDRB.W Rt, [Rn, #imm]
    INST_T32_LDRH_IMM,          // LDRH.W Rt, [Rn, #imm]
    INST_T32_LDRSB_IMM,         // LDRSB.W Rt, [Rn, #imm]
    INST_T32_LDRSH_IMM,         // LDRSH.W Rt, [Rn, #imm]
    INST_T32_STR_IMM,           // STR.W Rt, [Rn, #imm]
    INST_T32_STRB_IMM,          // STRB.W Rt, [Rn, #imm]
    INST_T32_STRH_IMM,          // STRH.W Rt, [Rn, #imm]
    INST_T32_STR_PRE_POST,      // STR.W Rt, [Rn, #imm]! or [Rn], #imm
    INST_T32_STRB_PRE_POST,     // STRB.W Rt, [Rn, #imm]! or [Rn], #imm
    INST_T32_STRH_PRE_POST,     // STRH.W Rt, [Rn, #imm]! or [Rn], #imm
    INST_T32_LDR_PRE_POST,      // LDR.W Rt, [Rn, #imm]! or [Rn], #imm
    INST_T32_LDRB_PRE_POST,     // LDRB.W Rt, [Rn, #imm]! or [Rn], #imm
    INST_T32_LDRSB_PRE_POST,    // LDRSB.W Rt, [Rn, #imm]! or [Rn], #imm
    INST_T32_LDRH_PRE_POST,     // LDRH.W Rt, [Rn, #imm]! or [Rn], #imm
    INST_T32_LDRSH_PRE_POST,    // LDRSH.W Rt, [Rn, #imm]! or [Rn], #imm
    INST_T32_LDRD,              // LDRD Rt, Rt2, [Rn, #imm]
    INST_T32_STRD,              // STRD Rt, Rt2, [Rn, #imm]
    
    // --- T32: Multiple Load/Store Instructions ---
    INST_T32_LDMIA,             // LDMIA.W Rn!, {reglist}
    INST_T32_LDMDB,             // LDMDB.W Rn!, {reglist}
    INST_T32_STMIA,             // STMIA.W Rn!, {reglist}
    INST_T32_STMDB,             // STMDB.W Rn!, {reglist}

#if HAS_EXCLUSIVE_ACCESS
    // --- T32: Exclusive Access Instructions (ARMv7-M+) ---
    INST_T32_LDREX,             // LDREX Rt, [Rn]
    INST_T32_STREX,             // STREX Rd, Rt, [Rn]
    INST_T32_LDREXB,            // LDREXB Rt, [Rn]
    INST_T32_STREXB,            // STREXB Rd, Rt, [Rn]
    INST_T32_LDREXH,            // LDREXH Rt, [Rn]
    INST_T32_STREXH,            // STREXH Rd, Rt, [Rn]
#endif

#if HAS_HARDWARE_DIVIDE
    // --- T32: Hardware Divide (ARMv7-M+) ---
    INST_T32_UDIV,              // UDIV Rd, Rn, Rm
    INST_T32_SDIV,              // SDIV Rd, Rn, Rm
    INST_T32_MUL,               // MUL Rd, Rn, Rm
    INST_T32_MLA,               // MLA Rd, Rn, Rm, Ra
    INST_T32_MLS,               // MLS Rd, Rn, Rm, Ra
    
    // --- T32: Long Multiply Instructions ---
    INST_T32_UMULL,             // UMULL RdLo, RdHi, Rn, Rm
    INST_T32_SMULL,             // SMULL RdLo, RdHi, Rn, Rm
    INST_T32_UMLAL,             // UMLAL RdLo, RdHi, Rn, Rm
    INST_T32_SMLAL,             // SMLAL RdLo, RdHi, Rn, Rm
#endif

#if HAS_BITFIELD_INSTRUCTIONS
    // --- T32: Bitfield Instructions (ARMv7-M+) ---
    INST_T32_BFI,               // BFI Rd, Rn, #lsb, #width
    INST_T32_BFC,               // BFC Rd, #lsb, #width
    INST_T32_UBFX,              // UBFX Rd, Rn, #lsb, #width
    INST_T32_SBFX,              // SBFX Rd, Rn, #lsb, #width
    
    // --- T32: Sign/Zero Extend Instructions ---
    INST_T32_SXTH,              // SXTH Rd, Rm
    INST_T32_SXTB,              // SXTB Rd, Rm  
    INST_T32_UXTH,              // UXTH Rd, Rm
    INST_T32_UXTB,              // UXTB Rd, Rm
#endif

#if HAS_SATURATING_ARITHMETIC
    // --- T32: Saturating Arithmetic (ARMv7-M+) ---
    INST_T32_SSAT,              // SSAT Rd, #imm, Rn
    INST_T32_USAT,              // USAT Rd, #imm, Rn
#endif

#if HAS_BIT_MANIPULATION
    // --- T32: Bit Manipulation Instructions (ARMv7-M+) ---
    INST_T32_CLZ,               // CLZ Rd, Rm (Count Leading Zeros)
    INST_T32_RBIT,              // RBIT Rd, Rm (Reverse Bits)
    INST_T32_REV,               // REV Rd, Rm (Byte-Reverse Word)
    INST_T32_REV16,             // REV16 Rd, Rm (Byte-Reverse Packed Halfword)
    INST_T32_REVSH,             // REVSH Rd, Rm (Byte-Reverse Signed Halfword)
#endif

    // --- T32: Coprocessor Instructions ---
    INST_T32_STC,               // STC coproc, CRd, [Rn]
    INST_T32_LDC,               // LDC coproc, CRd, [Rn]
    INST_T32_LDC_LIT,           // LDC coproc, CRd, label
    INST_T32_MCRR,              // MCRR coproc, opc1, Rt, Rt2, CRm
    INST_T32_MRRC,              // MRRC coproc, opc1, Rt, Rt2, CRm
    INST_T32_CDP,               // CDP coproc, opc1, CRd, CRn, CRm, opc2
    INST_T32_MCR,               // MCR coproc, opc1, Rt, CRn, CRm, opc2
    INST_T32_MRC,               // MRC coproc, opc1, Rt, CRn, CRm, opc2
    INST_T32_SYSTEM_CP,         // System coprocessor instruction

    // --- T32: Literal Load Instructions ---
    INST_T32_LDRD_LIT,          // LDRD Rt, Rt2, label
    INST_T32_LDRB_LIT,          // LDRB Rt, label
    INST_T32_LDRSB_LIT,         // LDRSB Rt, label
    INST_T32_LDRH_LIT,          // LDRH Rt, label
    INST_T32_LDRSH_LIT,         // LDRSH Rt, label
    INST_T32_LDR_LIT,           // LDR Rt, label
    INST_T32_PLD_LIT,           // PLD label
    INST_T32_PLI_LIT,           // PLI label

    // --- T32: Hint Instructions ---
    INST_T32_NOP,               // NOP.W
    INST_T32_SEV,               // SEV.W (Send Event)
    INST_T32_SEVL,              // SEVL.W (Send Event Local) 
    INST_T32_WFE,               // WFE.W (Wait for Event)
    INST_T32_WFI,               // WFI.W (Wait for Interrupt)
    INST_T32_YIELD,             // YIELD.W

    // --- T32: Store Instructions (Register) ---
    INST_T32_STRB_REG,          // STRB.W Rt, [Rn, Rm]
    INST_T32_STRH_REG,          // STRH.W Rt, [Rn, Rm]
    INST_T32_STR_REG,           // STR.W Rt, [Rn, Rm]

    // --- T32: Load Instructions (Register) ---
    INST_T32_LDRB_REG,          // LDRB.W Rt, [Rn, Rm]
    INST_T32_LDRH_REG,          // LDRH.W Rt, [Rn, Rm]
    INST_T32_LDR_REG,           // LDR.W Rt, [Rn, Rm]
    INST_T32_LDRSB_REG,         // LDRSB.W Rt, [Rn, Rm]
    INST_T32_LDRSH_REG,         // LDRSH.W Rt, [Rn, Rm]

    // --- T32: Memory Hint Instructions ---
    INST_T32_PLD_IMM,           // PLD [Rn, #imm]
    INST_T32_PLD_REG,           // PLD [Rn, Rm]
    INST_T32_PLI_IMM,           // PLI [Rn, #imm]
    INST_T32_PLI_REG,           // PLI [Rn, Rm]

    // --- T32: Shift Instructions (Immediate) ---

    // --- T32: Unprivileged Load/Store Instructions ---
    INST_T32_LDRSHT,            // LDRSHT Rt, [Rn, #imm]
    INST_T32_LDRT,              // LDRT Rt, [Rn, #imm]
#endif

#if SUPPORTS_ARMV7E_M
    // --- T32: Parallel Arithmetic Instructions ---

    INST_T32_SSAT16,            // SSAT16 Rd, #imm, Rn
    INST_T32_USAT16,            // USAT16 Rd, #imm, Rn

    // --- T32: Pack Instructions ---
    INST_T32_PKHBT,             // PKHBT Rd, Rn, Rm
    INST_T32_PKHTB,             // PKHTB Rd, Rn, Rm

    // --- T32: Sign/Zero Extend with Add Instructions ---
    INST_T32_SXTAH,             // SXTAH Rd, Rn, Rm
    INST_T32_UXTAH,             // UXTAH Rd, Rn, Rm
    INST_T32_SXTAB16,           // SXTAB16 Rd, Rn, Rm
    INST_T32_SXTB16,            // SXTB16 Rd, Rm
    INST_T32_UXTAB16,           // UXTAB16 Rd, Rn, Rm
    INST_T32_UXTB16,            // UXTB16 Rd, Rm
    INST_T32_SXTAB,             // SXTAB Rd, Rn, Rm
    INST_T32_UXTAB,             // UXTAB Rd, Rn, Rm

#if HAS_DSP_EXTENSIONS
    // --- T32: DSP Extensions (ARMv7E-M) ---
    INST_T32_QADD,              // QADD Rd, Rn, Rm
    INST_T32_QSUB,              // QSUB Rd, Rn, Rm
    INST_T32_QDADD,             // QDADD Rd, Rn, Rm
    INST_T32_QDSUB,             // QDSUB Rd, Rn, Rm

    // --- T32: Parallel Add/Subtract (non-saturating signed) ---
    INST_T32_SADD16,            // SADD16 Rd, Rn, Rm
    INST_T32_SSUB16,            // SSUB16 Rd, Rn, Rm
    INST_T32_SASX,              // SASX Rd, Rn, Rm
    INST_T32_SSAX,              // SSAX Rd, Rn, Rm
    INST_T32_SADD8,             // SADD8 Rd, Rn, Rm
    INST_T32_SSUB8,             // SSUB8 Rd, Rn, Rm

    // --- T32: Parallel Add/Subtract (saturating) ---
    INST_T32_QADD16,            // QADD16 Rd, Rn, Rm
    INST_T32_QSUB16,            // QSUB16 Rd, Rn, Rm
    INST_T32_QASX,              // QASX Rd, Rn, Rm
    INST_T32_QSAX,              // QSAX Rd, Rn, Rm
    INST_T32_QADD8,             // QADD8 Rd, Rn, Rm
    INST_T32_QSUB8,             // QSUB8 Rd, Rn, Rm

    // --- T32: Parallel Add/Subtract (Halving signed) ---
    INST_T32_SHADD16,            // SHADD16 Rd, Rn, Rm
    INST_T32_SHSUB16,            // SHSUB16 Rd, Rn, Rm
    INST_T32_SHASX,              // SHASX Rd, Rn, Rm
    INST_T32_SHSAX,              // SHSAX Rd, Rn, Rm
    INST_T32_SHADD8,             // SHADD8 Rd, Rn, Rm
    INST_T32_SHSUB8,             // SHSUB8 Rd, Rn, Rm

    // --- T32: Parallel Add/Subtract (non-saturating unsigned) ---
    INST_T32_UADD16,            // UADD16 Rd, Rn, Rm
    INST_T32_USUB16,            // USUB16 Rd, Rn, Rm
    INST_T32_UASX,              // UASX Rd, Rn, Rm
    INST_T32_USAX,              // USAX Rd, Rn, Rm
    INST_T32_UADD8,             // UADD8 Rd, Rn, Rm
    INST_T32_USUB8,             // USUB8 Rd, Rn, Rm

    // --- T32: Parallel Add/Subtract (non-saturating unsigned) ---
    INST_T32_UQADD16,            // UQADD16 Rd, Rn, Rm
    INST_T32_UQSUB16,            // UQSUB16 Rd, Rn, Rm
    INST_T32_UQASX,              // UQASX Rd, Rn, Rm
    INST_T32_UQSAX,              // UQSAX Rd, Rn, Rm
    INST_T32_UQADD8,             // UQADD8 Rd, Rn, Rm
    INST_T32_UQSUB8,             // UQSUB8 Rd, Rn, Rm

    // --- T32: Parallel Add/Subtract (Halving unsigned) ---
    INST_T32_UHADD16,            // UHADD16 Rd, Rn, Rm
    INST_T32_UHSUB16,            // UHSUB16 Rd, Rn, Rm
    INST_T32_UHASX,              // UHASX Rd, Rn, Rm
    INST_T32_UHSAX,              // UHSAX Rd, Rn, Rm
    INST_T32_UHADD8,             // UHADD8 Rd, Rn, Rm
    INST_T32_UHSUB8,             // UHSUB8 Rd, Rn, Rm

    INST_T32_SEL,               // SEL Rd, Rn, Rm (Select Bytes)
#endif

  // 16-bit x 16-bit = 32-bit 乘法指令
    INST_T32_SMLABB,           // Signed Multiply Accumulate (Bottom x Bottom)
    INST_T32_SMLABT,           // Signed Multiply Accumulate (Bottom x Top)  
    INST_T32_SMLATB,           // Signed Multiply Accumulate (Top x Bottom)
    INST_T32_SMLATT,           // Signed Multiply Accumulate (Top x Top)
    INST_T32_SMLAWB,           // Signed Multiply Accumulate Word (Bottom)
    INST_T32_SMLAWT,           // Signed Multiply Accumulate Word (Top)
    
    INST_T32_SMULBB,           // Signed Multiply (Bottom x Bottom)
    INST_T32_SMULBT,           // Signed Multiply (Bottom x Top)
    INST_T32_SMULTB,           // Signed Multiply (Top x Bottom)
    INST_T32_SMULTT,           // Signed Multiply (Top x Top)
    INST_T32_SMULWB,           // Signed Multiply Word (Bottom)
    INST_T32_SMULWT,           // Signed Multiply Word (Top)
    
    // 双32-bit乘法累加指令
    INST_T32_SMLAD,            // Signed Multiply Accumulate Dual
    INST_T32_SMLADX,           // Signed Multiply Accumulate Dual (Exchange)
    INST_T32_SMUAD,            // Signed Multiply Dual
    INST_T32_SMUADX,           // Signed Multiply Dual (Exchange)
    
    INST_T32_SMLSD,            // Signed Multiply Subtract Dual
    INST_T32_SMLSDX,           // Signed Multiply Subtract Dual (Exchange)
    INST_T32_SMUSD,            // Signed Multiply Subtract Dual
    INST_T32_SMUSDX,           // Signed Multiply Subtract Dual (Exchange)
    
    // Most Significant Word乘法指令
    INST_T32_SMMLA,            // Signed Most Significant Word Multiply Accumulate
    INST_T32_SMMLAR,           // Signed Most Significant Word Multiply Accumulate (Round)
    INST_T32_SMMUL,            // Signed Most Significant Word Multiply
    INST_T32_SMMULR,           // Signed Most Significant Word Multiply (Round)
    INST_T32_SMMLS,            // Signed Most Significant Word Multiply Subtract
    INST_T32_SMMLSR,           // Signed Most Significant Word Multiply Subtract (Round)
    
    // 无符号绝对差累加指令
    INST_T32_USAD8,            // Unsigned Sum of Absolute Differences
    INST_T32_USADA8,           // Unsigned Sum of Absolute Differences and Accumulate
    
    // 长乘法半字指令
    INST_T32_SMLALBB,          // Signed Multiply Accumulate Long (Bottom x Bottom)
    INST_T32_SMLALBT,          // Signed Multiply Accumulate Long (Bottom x Top)
    INST_T32_SMLALTB,          // Signed Multiply Accumulate Long (Top x Bottom)
    INST_T32_SMLALTT,          // Signed Multiply Accumulate Long (Top x Top)
    
    // 双乘法累加长指令
    INST_T32_SMLALD,           // Signed Multiply Accumulate Long Dual
    INST_T32_SMLALDX,          // Signed Multiply Accumulate Long Dual (Exchange)
    INST_T32_SMLSLD,           // Signed Multiply Subtract Long Dual
    INST_T32_SMLSLDX,          // Signed Multiply Subtract Long Dual (Exchange)
    
    // 无符号乘法累加累加长指令
    INST_T32_UMAAL,            // Unsigned Multiply Accumulate Accumulate Long

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
    bool pre_indexed;    // Pre-indexed (1) or post-indexed (0) addressing
    bool writeback;      // Writeback to base register
    bool negative_offset; // Negative offset for immediate addressing
    bool is_32bit;       // True if 32-bit instruction
    uint8_t addressing_mode; // Addressing mode for coprocessor instructions
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
    
    // T32 helper functions
    uint32_t decode_t32_modified_immediate(uint32_t i, uint32_t imm3, uint32_t imm8);
};

#endif // INSTRUCTION_H