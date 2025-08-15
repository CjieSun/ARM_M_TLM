#ifndef INSTRUCTION_H
#define INSTRUCTION_H

#include <systemc>
#include <cstdint>

using namespace sc_core;

// ARM instruction types
enum InstructionType {
    INST_UNKNOWN = 0,
    INST_BRANCH,
    INST_DATA_PROCESSING,
    INST_LOAD_STORE,
    INST_LOAD_STORE_MULTIPLE,
    INST_STATUS_REGISTER,
    INST_MISCELLANEOUS,
    INST_EXCEPTION
};

// Instruction fields structure
struct InstructionFields {
    uint16_t opcode;     // Raw 16-bit instruction
    uint8_t rd;          // Destination register
    uint8_t rn;          // First operand register  
    uint8_t rm;          // Second operand register
    uint8_t rs;          // Shift register
    uint16_t imm;        // Immediate value
    uint8_t cond;        // Condition code
    bool s_bit;          // Set flags bit
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
    // Decode specific instruction types
    InstructionFields decode_branch(uint16_t instruction);
    InstructionFields decode_data_processing(uint16_t instruction);
    InstructionFields decode_load_store(uint16_t instruction);
    InstructionFields decode_load_store_multiple(uint16_t instruction);
    InstructionFields decode_status_register(uint16_t instruction);
    InstructionFields decode_miscellaneous(uint16_t instruction);
    
    // Helper functions
    InstructionType identify_instruction_type(uint16_t instruction);
};

#endif // INSTRUCTION_H