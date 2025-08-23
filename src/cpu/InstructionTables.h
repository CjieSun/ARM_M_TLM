#ifndef INSTRUCTION_TABLES_H
#define INSTRUCTION_TABLES_H

#include "Instruction.h"
#include <cstdint>
#include <string>

// Structure to hold instruction format information
struct InstructionFormat {
    uint16_t mask;           // Bitmask to apply to instruction
    uint16_t pattern;        // Expected pattern after masking
    const char* description; // Human-readable description
    const char* format_name; // Format name (e.g., "Format 1")
    const char* encoding;    // Encoding pattern string (e.g., "000xxx")
};

// Structure to hold instruction encoding table entry
struct InstructionTableEntry {
    uint8_t opcode_bits;     // Number of opcode bits to examine
    uint16_t opcode_shift;   // Bit position to start examination
    uint16_t opcode_mask;    // Mask for opcode extraction
    InstructionType inst_type;
    const char* mnemonic;    // Assembly mnemonic
    const char* description; // Description
};

// Thumb-16 instruction format table
extern const InstructionFormat thumb16_formats[];
extern const size_t thumb16_formats_count;

// Thumb-16 Format 1: Shift (immediate) lookup table
extern const InstructionTableEntry thumb16_format1_table[];
extern const size_t thumb16_format1_count;

// Thumb-16 Format 4: ALU operations lookup table  
extern const InstructionTableEntry thumb16_format4_table[];
extern const size_t thumb16_format4_count;

// Thumb-16 Format 5: Hi register operations lookup table
extern const InstructionTableEntry thumb16_format5_table[];
extern const size_t thumb16_format5_count;

// Thumb-16 Format 7: Load/store register offset lookup table
extern const InstructionTableEntry thumb16_format7_table[];
extern const size_t thumb16_format7_count;

// Thumb-32 instruction format table
extern const InstructionFormat thumb32_formats[];
extern const size_t thumb32_formats_count;

// Function to generate instruction encoding tables
void generate_instruction_tables();

// Function to print instruction format tables
void print_thumb16_format_table();
void print_thumb32_format_table();

#endif // INSTRUCTION_TABLES_H