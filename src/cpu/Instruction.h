#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <systemc>
#include <cstdint>

using namespace sc_core;

// ARM instruction types for ARMv6-M Thumb instruction set
enum InstructionType {
    INST_UNKNOWN = 0,
    // Data processing
    INST_MOVE_SHIFTED_REG,      // Format 1: Move shifted register
    INST_ADD_SUB_REG_IMM,       // Format 2: Add/subtract register/immediate
    INST_MOV_CMP_ADD_SUB_IMM,   // Format 3: Move/compare/add/subtract immediate
    INST_ALU_OPERATIONS,        // Format 4: ALU operations
    INST_HI_REG_BX,            // Format 5: Hi register operations/branch exchange
    // Load/store
    INST_LOAD_STORE_REG_OFF,    // Format 6: Load/store with register offset
    INST_LOAD_STORE_SIGN_EXT,   // Format 7: Load/store sign-extended byte/halfword
    INST_LOAD_STORE_HALFWORD,   // Format 8: Load/store halfword
    INST_LOAD_STORE_IMM_OFF,    // Format 9: Load/store with immediate offset
    INST_LOAD_STORE_HW_IMM,     // Format 10: Load/store halfword immediate
    INST_LOAD_STORE_SP_REL,     // Format 11: SP-relative load/store
    INST_LOAD_ADDRESS,          // Format 12: Load address
    // Stack and multiple
    INST_ADD_SP_IMM,           // Format 13: Add offset to Stack Pointer
    INST_PUSH_POP,             // Format 14: Push/pop registers
    INST_LOAD_STORE_MULTIPLE,   // Format 15: Multiple load/store
    // Branch and exception
    INST_BRANCH_COND,          // Format 16: Conditional branch (was INST_BRANCH)
    INST_SWI,                  // Format 17: Software interrupt (was INST_EXCEPTION)
    INST_BRANCH_UNCOND,        // Format 18: Unconditional branch
    INST_BRANCH_LINK,          // Format 19: Long branch with link
    // Legacy compatibility
    INST_BRANCH = INST_BRANCH_COND,
    INST_DATA_PROCESSING = INST_ALU_OPERATIONS,
    INST_LOAD_STORE = INST_LOAD_STORE_IMM_OFF,
    INST_STATUS_REGISTER = INST_UNKNOWN,  // Not used in ARMv6-M Thumb
    INST_MISCELLANEOUS = INST_ADD_SP_IMM,
    INST_EXCEPTION = INST_SWI
};

// Instruction fields structure
struct InstructionFields {
    uint16_t opcode;     // Raw 16-bit instruction
    uint8_t rd;          // Destination register
    uint8_t rn;          // First operand register  
    uint8_t rm;          // Second operand register
    uint8_t rs;          // Shift register / third operand
    uint16_t imm;        // Immediate value
    uint8_t cond;        // Condition code
    bool s_bit;          // Set flags bit
    uint8_t shift_type;  // Shift type (LSL, LSR, ASR, ROR)
    uint8_t shift_amount; // Shift amount
    uint8_t alu_op;      // ALU operation code
    bool h1, h2;         // Hi register flags
    uint16_t reg_list;   // Register list for multiple load/store
    bool load_store_bit; // Load (1) or Store (0)
    uint8_t byte_word;   // Byte (1) or Word (0)
    InstructionType type;
};

class Instruction : public sc_module
{
public:
    // Constructor
    SC_HAS_PROCESS(Instruction);
    Instruction(sc_module_name name);
    
    // Decode instruction
    InstructionFields decode(uint16_t instruction);
    
    // Check if instruction is 32-bit (Thumb-2)
    bool is_32bit_instruction(uint16_t first_half);
    
    // Get instruction name for logging
    const char* get_instruction_name(const InstructionFields& fields);
    
private:
    // Decode specific instruction types (ARMv6-M Thumb formats)
    InstructionFields decode_move_shifted_reg(uint16_t instruction);        // Format 1
    InstructionFields decode_add_sub_reg_imm(uint16_t instruction);         // Format 2
    InstructionFields decode_mov_cmp_add_sub_imm(uint16_t instruction);     // Format 3
    InstructionFields decode_alu_operations(uint16_t instruction);          // Format 4
    InstructionFields decode_hi_reg_bx(uint16_t instruction);               // Format 5
    InstructionFields decode_load_store_reg_off(uint16_t instruction);      // Format 6
    InstructionFields decode_load_store_sign_ext(uint16_t instruction);     // Format 7
    InstructionFields decode_load_store_halfword(uint16_t instruction);     // Format 8
    InstructionFields decode_load_store_imm_off(uint16_t instruction);      // Format 9
    InstructionFields decode_load_store_sp_rel(uint16_t instruction);       // Format 11
    InstructionFields decode_load_address(uint16_t instruction);            // Format 12
    InstructionFields decode_add_sp_imm(uint16_t instruction);              // Format 13
    InstructionFields decode_push_pop(uint16_t instruction);                // Format 14
    InstructionFields decode_load_store_multiple(uint16_t instruction);     // Format 15
    InstructionFields decode_branch_cond(uint16_t instruction);             // Format 16
    InstructionFields decode_swi(uint16_t instruction);                     // Format 17
    InstructionFields decode_branch_uncond(uint16_t instruction);           // Format 18
    InstructionFields decode_branch_link(uint16_t instruction);             // Format 19
    
    // Legacy decode methods for backward compatibility
    InstructionFields decode_branch(uint16_t instruction);
    InstructionFields decode_data_processing(uint16_t instruction);
    InstructionFields decode_load_store(uint16_t instruction);
    InstructionFields decode_status_register(uint16_t instruction);
    InstructionFields decode_miscellaneous(uint16_t instruction);
    
    // Helper functions
    InstructionType identify_instruction_type(uint16_t instruction);
};

#endif // INSTRUCTION_H