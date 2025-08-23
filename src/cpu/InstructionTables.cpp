#include "InstructionTables.h"
#include "Log.h"
#include <iostream>
#include <iomanip>

// Thumb-16 instruction format identification table
const InstructionFormat thumb16_formats[] = {
    // Format 1: Shift (immediate)
    {0xE000, 0x0000, "Shift (immediate), add, subtract, move, and compare", "Format 1", "000xxx"},
    // Format 2: Add/subtract (part of Format 1 range)
    {0xF800, 0x1800, "Add/subtract", "Format 2", "000110"},
    {0xF800, 0x1C00, "Add/subtract", "Format 2", "000111"},
    // Format 3: Move/compare/add/subtract immediate  
    {0xE000, 0x2000, "Move/compare/add/subtract immediate", "Format 3", "001xxx"},
    // Format 4: ALU operations
    {0xFC00, 0x4000, "Data processing", "Format 4", "010000"},
    // Format 5: Hi register operations/branch exchange
    {0xFC00, 0x4400, "Special data instructions and branch and exchange", "Format 5", "010001"},
    // Format 6: PC-relative load
    {0xF800, 0x4800, "Load from Literal Pool", "Format 6", "01001x"},
    // Format 7: Load/store with register offset
    {0xF000, 0x5000, "Load/store single data item", "Format 7", "0101xx"},
    // Format 8: Load/store with immediate offset (word/byte)
    {0xE000, 0x6000, "Load/store single data item", "Format 8", "011xxx"},
    // Format 9: Load/store halfword
    {0xF000, 0x8000, "Load/store single data item", "Format 9", "1000xx"},
    // Format 10: SP-relative load/store
    {0xF000, 0x9000, "Generate PC-relative address", "Format 10", "1001xx"},
    // Format 11: Load address (PC/SP relative)
    {0xF000, 0xA000, "Generate SP-relative address", "Format 11", "1010xx"},
    // Format 12: ADD/SUB to SP
    {0xFF00, 0xB000, "Miscellaneous 16-bit instructions", "Format 12", "1011xx"},
    // Format 13: Push/pop registers
    {0xF600, 0xB400, "Store multiple registers", "Format 13a", "11000x"},
    {0xF600, 0xBC00, "Load multiple registers", "Format 13b", "11001x"},
    // Format 14: Conditional branch
    {0xF000, 0xD000, "Conditional branch, and Supervisor Call", "Format 14", "1101xx"},
    // Format 15: Unconditional branch
    {0xF800, 0xE000, "Unconditional Branch", "Format 15", "11100x"},
    // Format 16: Long branch with link (first half)
    {0xF000, 0xF000, "32-bit instruction prefixes", "Format 16", "1111xx"}
};

const size_t thumb16_formats_count = sizeof(thumb16_formats) / sizeof(thumb16_formats[0]);

// Thumb-16 Format 1: Shift operations lookup table
const InstructionTableEntry thumb16_format1_table[] = {
    {2, 11, 0x3, INST_T16_LSL_IMM, "LSL", "Logical shift left (immediate)"},
    {2, 11, 0x3, INST_T16_LSR_IMM, "LSR", "Logical shift right (immediate)"},
    {2, 11, 0x3, INST_T16_ASR_IMM, "ASR", "Arithmetic shift right (immediate)"}
};

const size_t thumb16_format1_count = sizeof(thumb16_format1_table) / sizeof(thumb16_format1_table[0]);

// Thumb-16 Format 4: ALU operations lookup table
const InstructionTableEntry thumb16_format4_table[] = {
    {4, 6, 0xF, INST_T16_AND,     "AND",  "Bitwise AND"},
    {4, 6, 0xF, INST_T16_EOR,     "EOR",  "Bitwise exclusive OR"},
    {4, 6, 0xF, INST_T16_LSL_REG, "LSL",  "Logical shift left"},
    {4, 6, 0xF, INST_T16_LSR_REG, "LSR",  "Logical shift right"},
    {4, 6, 0xF, INST_T16_ASR_REG, "ASR",  "Arithmetic shift right"},
    {4, 6, 0xF, INST_T16_ADC,     "ADC",  "Add with carry"},
    {4, 6, 0xF, INST_T16_SBC,     "SBC",  "Subtract with carry"},
    {4, 6, 0xF, INST_T16_ROR,     "ROR",  "Rotate right"},
    {4, 6, 0xF, INST_T16_TST,     "TST",  "Test"},
    {4, 6, 0xF, INST_T16_NEG,     "NEG",  "Negate"},
    {4, 6, 0xF, INST_T16_CMP_REG, "CMP",  "Compare"},
    {4, 6, 0xF, INST_T16_CMN,     "CMN",  "Compare negative"},
    {4, 6, 0xF, INST_T16_ORR,     "ORR",  "Bitwise OR"},
    {4, 6, 0xF, INST_T16_MUL,     "MUL",  "Multiply"},
    {4, 6, 0xF, INST_T16_BIC,     "BIC",  "Bit clear"},
    {4, 6, 0xF, INST_T16_MVN,     "MVN",  "Bitwise NOT"}
};

const size_t thumb16_format4_count = sizeof(thumb16_format4_table) / sizeof(thumb16_format4_table[0]);

// Thumb-16 Format 5: Hi register operations lookup table
const InstructionTableEntry thumb16_format5_table[] = {
    {2, 8, 0x3, INST_T16_ADD_HI, "ADD", "Add high registers"},
    {2, 8, 0x3, INST_T16_CMP_HI, "CMP", "Compare high registers"},
    {2, 8, 0x3, INST_T16_MOV_HI, "MOV", "Move high registers"},
    {2, 8, 0x3, INST_T16_BX,     "BX",  "Branch and exchange"},
    {2, 8, 0x3, INST_T16_BLX,    "BLX", "Branch with link and exchange"}
};

const size_t thumb16_format5_count = sizeof(thumb16_format5_table) / sizeof(thumb16_format5_table[0]);

// Thumb-16 Format 7: Load/store register offset lookup table
const InstructionTableEntry thumb16_format7_table[] = {
    {3, 9, 0x7, INST_T16_STR_REG,   "STR",   "Store word (register offset)"},
    {3, 9, 0x7, INST_T16_STRH_REG,  "STRH",  "Store halfword (register offset)"},
    {3, 9, 0x7, INST_T16_STRB_REG,  "STRB",  "Store byte (register offset)"},
    {3, 9, 0x7, INST_T16_LDRSB_REG, "LDRSB", "Load signed byte (register offset)"},
    {3, 9, 0x7, INST_T16_LDR_REG,   "LDR",   "Load word (register offset)"},
    {3, 9, 0x7, INST_T16_LDRH_REG,  "LDRH",  "Load halfword (register offset)"},
    {3, 9, 0x7, INST_T16_LDRB_REG,  "LDRB",  "Load byte (register offset)"},
    {3, 9, 0x7, INST_T16_LDRSH_REG, "LDRSH", "Load signed halfword (register offset)"}
};

const size_t thumb16_format7_count = sizeof(thumb16_format7_table) / sizeof(thumb16_format7_table[0]);

// Thumb-32 instruction format identification table
const InstructionFormat thumb32_formats[] = {
    // BL: Branch with link
    {0xF800, 0xF000, "Branch with Link", "T32 BL", "11110x"}
};

const size_t thumb32_formats_count = sizeof(thumb32_formats) / sizeof(thumb32_formats[0]);

void generate_instruction_tables() {
    LOG_INFO("Generating instruction encoding tables...");
    print_thumb16_format_table();
    print_thumb32_format_table();
}

void print_thumb16_format_table() {
    std::cout << "\n16-bit Thumb instruction encoding\n";
    std::cout << "==================================\n\n";
    std::cout << "Table: 16-bit Thumb instruction encoding\n";
    std::cout << std::setw(8) << "opcode" << " | " << std::setw(60) << "Instruction or instruction class" << "\n";
    std::cout << std::string(75, '-') << "\n";
    
    for (size_t i = 0; i < thumb16_formats_count; i++) {
        std::cout << std::setw(8) << thumb16_formats[i].encoding << " | " 
                  << std::setw(60) << thumb16_formats[i].description << "\n";
    }
    
    // Print Format 4 (ALU operations) detailed table
    std::cout << "\nFormat 4: Data processing operations (opcode 010000)\n";
    std::cout << std::setw(8) << "op[3:0]" << " | " << std::setw(8) << "Mnemonic" << " | " 
              << std::setw(40) << "Description" << "\n";
    std::cout << std::string(60, '-') << "\n";
    
    for (size_t i = 0; i < thumb16_format4_count; i++) {
        std::cout << std::setw(8) << std::dec << i << " | " 
                  << std::setw(8) << thumb16_format4_table[i].mnemonic << " | "
                  << std::setw(40) << thumb16_format4_table[i].description << "\n";
    }
    
    // Print Format 7 (Load/store register) detailed table
    std::cout << "\nFormat 7: Load/store single data item (register offset) (opcode 0101xx)\n";
    std::cout << std::setw(8) << "op[2:0]" << " | " << std::setw(8) << "Mnemonic" << " | " 
              << std::setw(40) << "Description" << "\n";
    std::cout << std::string(60, '-') << "\n";
    
    for (size_t i = 0; i < thumb16_format7_count; i++) {
        std::cout << std::setw(8) << std::dec << i << " | " 
                  << std::setw(8) << thumb16_format7_table[i].mnemonic << " | "
                  << std::setw(40) << thumb16_format7_table[i].description << "\n";
    }
}

void print_thumb32_format_table() {
    std::cout << "\n32-bit Thumb instruction encoding\n";
    std::cout << "==================================\n\n";
    std::cout << "Table: 32-bit Thumb instruction encoding\n";
    std::cout << std::setw(8) << "opcode" << " | " << std::setw(60) << "Instruction or instruction class" << "\n";
    std::cout << std::string(75, '-') << "\n";
    
    for (size_t i = 0; i < thumb32_formats_count; i++) {
        std::cout << std::setw(8) << thumb32_formats[i].encoding << " | " 
                  << std::setw(60) << thumb32_formats[i].description << "\n";
    }
}