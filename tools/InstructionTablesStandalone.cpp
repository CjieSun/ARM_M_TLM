#include "InstructionTablesStandalone.h"
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

// Thumb-16 Format 4: ALU operations lookup table
const InstructionTableEntry thumb16_format4_table[] = {
    {0,  "AND", "Bitwise AND"},
    {1,  "EOR", "Bitwise exclusive OR"},
    {2,  "LSL", "Logical shift left"},
    {3,  "LSR", "Logical shift right"},
    {4,  "ASR", "Arithmetic shift right"},
    {5,  "ADC", "Add with carry"},
    {6,  "SBC", "Subtract with carry"},
    {7,  "ROR", "Rotate right"},
    {8,  "TST", "Test"},
    {9,  "NEG", "Negate"},
    {10, "CMP", "Compare"},
    {11, "CMN", "Compare negative"},
    {12, "ORR", "Bitwise OR"},
    {13, "MUL", "Multiply"},
    {14, "BIC", "Bit clear"},
    {15, "MVN", "Bitwise NOT"}
};

const size_t thumb16_format4_count = sizeof(thumb16_format4_table) / sizeof(thumb16_format4_table[0]);

// Thumb-16 Format 7: Load/store register offset lookup table
const InstructionTableEntry thumb16_format7_table[] = {
    {0, "STR",   "Store word (register offset)"},
    {1, "STRH",  "Store halfword (register offset)"},
    {2, "STRB",  "Store byte (register offset)"},
    {3, "LDRSB", "Load signed byte (register offset)"},
    {4, "LDR",   "Load word (register offset)"},
    {5, "LDRH",  "Load halfword (register offset)"},
    {6, "LDRB",  "Load byte (register offset)"},
    {7, "LDRSH", "Load signed halfword (register offset)"}
};

const size_t thumb16_format7_count = sizeof(thumb16_format7_table) / sizeof(thumb16_format7_table[0]);

// Thumb-32 instruction format identification table
const InstructionFormat thumb32_formats[] = {
    // BL: Branch with link
    {0xF800, 0xF000, "Branch with Link", "T32 BL", "11110x"}
};

const size_t thumb32_formats_count = sizeof(thumb32_formats) / sizeof(thumb32_formats[0]);

void generate_instruction_tables() {
    std::cout << "Generating instruction encoding tables...\n";
    print_thumb16_format_table();
    print_thumb32_format_table();
}

void print_thumb16_format_table() {
    std::cout << "\n16-bit Thumb instruction encoding\n";
    std::cout << "==================================\n\n";
    std::cout << "The encoding of 16-bit Thumb instructions is:\n";
    std::cout << "15 14 13 12 11 10  9  8  7  6  5  4  3  2  1  0\n";
    std::cout << "|_____________________opcode____________________|\n\n";
    
    std::cout << "Table: 16-bit Thumb instruction encoding\n";
    std::cout << std::setw(8) << std::left << "opcode" << " | " << std::setw(60) << std::left << "Instruction or instruction class" << "\n";
    std::cout << std::string(75, '-') << "\n";
    
    for (size_t i = 0; i < thumb16_formats_count; i++) {
        std::cout << std::setw(8) << std::left << thumb16_formats[i].encoding << " | " 
                  << std::setw(60) << std::left << thumb16_formats[i].description << "\n";
    }
    
    // Print Format 4 (ALU operations) detailed table
    std::cout << "\nFormat 4: Data processing operations (opcode 010000)\n";
    std::cout << std::string(60, '=') << "\n";
    std::cout << std::setw(8) << std::left << "op[3:0]" << " | " << std::setw(8) << std::left << "Mnemonic" << " | " 
              << std::setw(40) << std::left << "Description" << "\n";
    std::cout << std::string(60, '-') << "\n";
    
    for (size_t i = 0; i < thumb16_format4_count; i++) {
        std::cout << std::setw(8) << std::left << ("0x" + std::to_string(thumb16_format4_table[i].opcode_value)) << " | " 
                  << std::setw(8) << std::left << thumb16_format4_table[i].mnemonic << " | "
                  << std::setw(40) << std::left << thumb16_format4_table[i].description << "\n";
    }
    
    // Print Format 7 (Load/store register) detailed table
    std::cout << "\nFormat 7: Load/store single data item (register offset) (opcode 0101xx)\n";
    std::cout << std::string(65, '=') << "\n";
    std::cout << std::setw(8) << std::left << "op[2:0]" << " | " << std::setw(8) << std::left << "Mnemonic" << " | " 
              << std::setw(40) << std::left << "Description" << "\n";
    std::cout << std::string(60, '-') << "\n";
    
    for (size_t i = 0; i < thumb16_format7_count; i++) {
        std::cout << std::setw(8) << std::left << ("0x" + std::to_string(thumb16_format7_table[i].opcode_value)) << " | " 
                  << std::setw(8) << std::left << thumb16_format7_table[i].mnemonic << " | "
                  << std::setw(40) << std::left << thumb16_format7_table[i].description << "\n";
    }
}

void print_thumb32_format_table() {
    std::cout << "\n32-bit Thumb instruction encoding\n";
    std::cout << "==================================\n\n";
    std::cout << "Table: 32-bit Thumb instruction encoding\n";
    std::cout << std::setw(8) << std::left << "opcode" << " | " << std::setw(60) << std::left << "Instruction or instruction class" << "\n";
    std::cout << std::string(75, '-') << "\n";
    
    for (size_t i = 0; i < thumb32_formats_count; i++) {
        std::cout << std::setw(8) << std::left << thumb32_formats[i].encoding << " | " 
                  << std::setw(60) << std::left << thumb32_formats[i].description << "\n";
    }
    std::cout << "\nNote: ARMv6-M supports only BL (Branch with Link) as 32-bit instruction.\n";
}