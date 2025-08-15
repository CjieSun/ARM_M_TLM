#include "Instruction.h"
#include "Log.h"

Instruction::Instruction(sc_module_name name) : sc_module(name)
{
    LOG_INFO("Instruction decoder initialized");
}

InstructionFields Instruction::decode(uint16_t instruction)
{
    InstructionFields fields = {};
    fields.opcode = instruction;
    fields.type = identify_instruction_type(instruction);
    
    // Decode based on instruction type
    switch (fields.type) {
        case INST_BRANCH:
            return decode_branch(instruction);
        case INST_DATA_PROCESSING:
            return decode_data_processing(instruction);
        case INST_LOAD_STORE:
            return decode_load_store(instruction);
        case INST_LOAD_STORE_MULTIPLE:
            return decode_load_store_multiple(instruction);
        case INST_STATUS_REGISTER:
            return decode_status_register(instruction);
        case INST_MISCELLANEOUS:
            return decode_miscellaneous(instruction);
        default:
            fields.type = INST_UNKNOWN;
            break;
    }
    
    return fields;
}

InstructionType Instruction::identify_instruction_type(uint16_t instruction)
{
    // Basic Thumb instruction identification
    // This is a simplified version - real ARM would have more complex decoding
    
    if ((instruction & 0xF000) == 0xD000) {
        return INST_BRANCH;  // B<cond>
    }
    
    if ((instruction & 0xF800) == 0xE000) {
        return INST_BRANCH;  // B (unconditional)
    }
    
    if ((instruction & 0xF000) == 0x4000) {
        return INST_DATA_PROCESSING;  // Data processing
    }
    
    if ((instruction & 0xF000) == 0x6000 || (instruction & 0xF000) == 0x8000) {
        return INST_LOAD_STORE;  // Load/Store
    }
    
    if ((instruction & 0xF000) == 0xC000) {
        return INST_LOAD_STORE_MULTIPLE;  // LDM/STM
    }
    
    if ((instruction & 0xFF00) == 0xDF00) {
        return INST_EXCEPTION;  // SVC
    }
    
    return INST_DATA_PROCESSING;  // Default to data processing
}

InstructionFields Instruction::decode_branch(uint16_t instruction)
{
    InstructionFields fields = {};
    fields.opcode = instruction;
    fields.type = INST_BRANCH;
    
    if ((instruction & 0xF000) == 0xD000) {
        // B<cond> instruction
        fields.cond = (instruction >> 8) & 0xF;
        fields.imm = instruction & 0xFF;
        if (fields.imm & 0x80) {
            fields.imm |= 0xFF00;  // Sign extend
        }
    } else if ((instruction & 0xF800) == 0xE000) {
        // B instruction (unconditional)
        fields.cond = 0xE;  // Always condition
        fields.imm = instruction & 0x7FF;
        if (fields.imm & 0x400) {
            fields.imm |= 0xF800;  // Sign extend
        }
    }
    
    return fields;
}

InstructionFields Instruction::decode_data_processing(uint16_t instruction)
{
    InstructionFields fields = {};
    fields.opcode = instruction;
    fields.type = INST_DATA_PROCESSING;
    
    // Simplified data processing decode
    fields.rd = instruction & 0x7;
    fields.rn = (instruction >> 3) & 0x7;
    fields.rm = (instruction >> 6) & 0x7;
    
    return fields;
}

InstructionFields Instruction::decode_load_store(uint16_t instruction)
{
    InstructionFields fields = {};
    fields.opcode = instruction;
    fields.type = INST_LOAD_STORE;
    
    // Simplified load/store decode
    fields.rd = instruction & 0x7;
    fields.rn = (instruction >> 3) & 0x7;
    fields.imm = (instruction >> 6) & 0x1F;
    
    return fields;
}

InstructionFields Instruction::decode_load_store_multiple(uint16_t instruction)
{
    InstructionFields fields = {};
    fields.opcode = instruction;
    fields.type = INST_LOAD_STORE_MULTIPLE;
    
    fields.rn = (instruction >> 8) & 0x7;
    fields.imm = instruction & 0xFF;  // Register list
    
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