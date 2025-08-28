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
    
    // Helper methods
    uint32_t compute_operand2(const InstructionFields& fields);
    void update_flags(uint32_t result, bool carry, bool overflow);
    bool check_condition(uint8_t condition);
    
    // Memory access helpers
    uint32_t read_memory(uint32_t address, uint32_t size, void* socket);
    void write_memory(uint32_t address, uint32_t data, uint32_t size, void* socket);
};

#endif // EXECUTE_H