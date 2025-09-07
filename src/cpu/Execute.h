#ifndef EXECUTE_H
#define EXECUTE_H

#include <systemc>
#include <tlm>
#include <tlm_utils/simple_initiator_socket.h>
#include <cstdint>
#include "Instruction.h"
#include "Registers.h"
// Forward declare CPU to avoid circular dependency in headers
class CPU;

using namespace sc_core;
using namespace tlm;

// Forward declaration to avoid circular dependency
template<typename T> 
using simple_initiator_socket_base = tlm_utils::simple_initiator_socket<T>;

class Execute : public sc_module
{
public:
    // Constructor
    SC_HAS_PROCESS(Execute);
    Execute(sc_module_name name, Registers* registers);
    // Wire back to CPU to trigger synchronous exceptions (e.g., SVC)
    void set_cpu(CPU* cpu) { m_cpu = cpu; }
    
    // Execute instruction - using void* to avoid circular dependency
    bool execute_instruction(const InstructionFields& fields, void* data_bus);
    
private:
    Registers* m_registers;
    CPU* m_cpu { nullptr };
    // DMI cache for data bus accesses from Execute
    bool m_data_dmi_valid { false };
    tlm_dmi m_data_dmi;
    
#if HAS_EXCLUSIVE_ACCESS
    // Exclusive access monitor state
    bool m_exclusive_monitor_enabled { false };
    uint32_t m_exclusive_address { 0 };
    uint32_t m_exclusive_size { 0 };  // 1, 2, or 4 bytes
#endif
    
    // Execution methods for different instruction types
    bool execute_branch(const InstructionFields& fields);
    bool execute_data_processing(const InstructionFields& fields);
    bool execute_load_store(const InstructionFields& fields, void* data_bus);
    bool execute_load_store_multiple(const InstructionFields& fields, void* data_bus);
    bool execute_status_register(const InstructionFields& fields);
    bool execute_miscellaneous(const InstructionFields& fields);
    bool execute_exception(const InstructionFields& fields);
    bool execute_extend(const InstructionFields& fields);
    bool execute_cps(const InstructionFields& fields);
    bool execute_memory_barrier(const InstructionFields& fields);
    bool execute_msr(const InstructionFields& fields);
    bool execute_mrs(const InstructionFields& fields);
    
#if HAS_CBZ_CBNZ
    // ARMv7-M Compare and Branch execution
    bool execute_cbz_cbnz(const InstructionFields& fields);
#endif
#if HAS_IT_BLOCKS
    // ARMv7-M If-Then block execution
    bool execute_it(const InstructionFields& fields);
#endif
#if HAS_EXTENDED_HINTS
    // ARMv7-M Extended Hint execution
    bool execute_extended_hint(const InstructionFields& fields);
#endif
    
#if SUPPORTS_ARMV7_M
    // ARMv7-M T32 instruction execution functions
    bool execute_table_branch(const InstructionFields& fields, void* data_bus);
    bool execute_clrex(const InstructionFields& fields);
    bool execute_t32_data_processing(const InstructionFields& fields);
    bool execute_t32_shift_register(const InstructionFields& fields);
    bool execute_t32_load_store(const InstructionFields& fields, void* data_bus);
    bool execute_t32_dual_load_store(const InstructionFields& fields, void* data_bus);
    bool execute_t32_multiple_load_store(const InstructionFields& fields, void* data_bus);
#if HAS_EXCLUSIVE_ACCESS
    bool execute_exclusive_load(const InstructionFields& fields, void* data_bus);
    bool execute_exclusive_store(const InstructionFields& fields, void* data_bus);
#endif
#if HAS_HARDWARE_DIVIDE
    bool execute_divide(const InstructionFields& fields);
    bool execute_mla(const InstructionFields& fields);
    bool execute_mls(const InstructionFields& fields);
    bool execute_long_multiply(const InstructionFields& fields);
#endif
#if HAS_BITFIELD_INSTRUCTIONS
    bool execute_bitfield(const InstructionFields& fields);
#endif
#if HAS_SATURATING_ARITHMETIC
    bool execute_saturate(const InstructionFields& fields);
#endif
#if HAS_BIT_MANIPULATION
    bool execute_bit_manipulation(const InstructionFields& fields);
#endif
#endif // SUPPORTS_ARMV7_M
    
    // Helper methods
    uint32_t compute_operand2(const InstructionFields& fields);
    void update_flags(uint32_t result, bool carry, bool overflow);
    bool check_condition(uint8_t condition);
    
    // Shift and formatting helpers
    uint32_t apply_shift(uint32_t value, uint8_t shift_type, uint8_t shift_amount);
    
    // Memory access helpers
    uint32_t read_memory(uint32_t address, uint32_t size, void* socket);
    void write_memory(uint32_t address, uint32_t data, uint32_t size, void* socket);
};

#endif // EXECUTE_H