# ARMv7-M Instruction Set Test Program

## Overview

The `armv7m_test.s` file is a comprehensive test program for ARMv7-M (Cortex-M3/M4) specific instructions. It tests various instruction categories that are available in ARMv7-M but not in ARMv6-M (Cortex-M0/M0+).

## Tested Instruction Categories

### 1. IT (If-Then) Blocks
- **IT EQ/NE/LT/GT** - Conditional execution blocks
- **ITE** - If-Then-Else blocks  
- **ITEE** - If-Then-Else-Else blocks
- Tests proper condition flag handling and execution gating

### 2. Byte Load/Store Operations
- **STRB/LDRB** with immediate and register offsets
- Verification of byte-only operations (not loading/storing full words)
- Pre-indexed and post-indexed addressing modes

### 3. Multiple Load/Store Operations
- **STMIA/LDMIA** with writeback
- Multiple register transfer with increment after
- Stack operations and register list handling

### 4. Multiply Instructions
- **UMULL** - Unsigned Multiply Long (64-bit result)
- **MLA** - Multiply Accumulate
- **MLS** - Multiply Subtract
- **MUL** - Basic 32-bit multiply

### 5. T32 Data Processing Instructions
- **ADD.W/SUB.W** - 32-bit wide arithmetic with large immediates
- **MOV.W/MOVT** - 32-bit immediate moves
- **AND.W/ORR.W/EOR.W/BIC.W** - Logical operations
- **ADD.W/SUB.W** with shifted register operands
- **CMP.W/TST.W** - Extended compare operations

### 6. Bitfield Instructions
- **BFI** - Bit Field Insert
- **BFC** - Bit Field Clear
- **UBFX** - Unsigned Bit Field Extract
- **SBFX** - Signed Bit Field Extract

### 7. Division Instructions
- **UDIV** - Unsigned Division
- **SDIV** - Signed Division
- Comprehensive testing including edge cases

### 8. Shift Instructions
- **LSL.W** - Logical Shift Left (register amount)
- **LSR.W** - Logical Shift Right (register amount)
- **ASR.W** - Arithmetic Shift Right (register amount)
- **ROR.W** - Rotate Right (register amount)

### 9. System Instructions
- **MRS** - Move from Special Register (PSP, MSP, PRIMASK, BASEPRI, FAULTMASK, CONTROL)
- **MSR** - Move to Special Register
- Exception masking and priority control

### 10. Advanced Load/Store
- **LDR.W/STR.W** with various addressing modes
- Pre-indexed: `[Rn, #imm]!`
- Post-indexed: `[Rn], #imm`
- Negative offsets: `[Rn, #-imm]`
- **LDRB.W/STRB.W** with indexing
- **LDRH.W/STRH.W** - Halfword operations

### 11. Wide Branch Instructions
- **B.W** - 32-bit branch for extended range
- **BEQ.W/BNE.W** - Conditional branches with extended range
- **BL.W** - Branch with Link (extended range)

### 12. Extend Instructions
- **UXTB** - Zero extend byte to word
- **UXTH** - Zero extend halfword to word  
- **SXTB** - Sign extend byte to word
- **SXTH** - Sign extend halfword to word

### 13. Reverse Instructions
- **REV** - Reverse bytes in word
- **REV16** - Reverse bytes in each halfword
- **REVSH** - Reverse bytes in signed halfword
- **RBIT** - Reverse bits in word (if supported)

### 14. Saturating Arithmetic
- **QADD** - Saturating Add
- **QSUB** - Saturating Subtract
- **SSAT** - Signed Saturate
- **USAT** - Unsigned Saturate

## Usage

### Building the Test
```bash
# Navigate to tests/assembly directory
cd tests/assembly

# Build ARMv7-M test specifically
make armv7m_test.hex

# Or build all tests
make all
```

### Running the Test
```bash
# Run the ARMv7-M test
make run-armv7m_test

# Or use the simulator directly
../../build/bin/arm_m_tlm --hex armv7m_test.hex --debug --trace
```

### Generating Reports
```bash
# Generate comprehensive test report
make test-report

# Generate HTML report only
make test-report-html

# Generate JSON report only  
make test-report-json
```

## Test Structure

Each test function follows this pattern:
1. **Setup** - Initialize test data and registers
2. **Execute** - Perform the instruction(s) being tested
3. **Verify** - Check results against expected values
4. **Report** - Return success (0x99) or failure (0xBA) code

The main test flow:
```
_start → test_it_blocks → test_byte_operations → ... → infinite_loop
```

## Success/Failure Codes

- **0x99** - Test section passed successfully
- **0xBA** - Test section failed (Bad)
- **0x00** - All tests completed successfully

## Memory Layout

- **test_buffer** - 128-byte aligned buffer for load/store tests
- **test_constants** - Pre-defined test data constants
- Stack grows downward from 4KB offset

## Debugging

Use GDB for step-by-step debugging:
```bash
# Start simulator with GDB support
../../build/bin/arm_m_tlm --hex armv7m_test.hex --gdb

# In another terminal, connect GDB
make gdb-armv7m_test
```

### 19. Memory Barrier Instructions
- **DSB** - Data Synchronization Barrier
- **DMB** - Data Memory Barrier  
- **ISB** - Instruction Synchronization Barrier
- Different barrier domains (SY, ISH, etc.)

### 20. Exclusive Memory Access
- **LDREX/STREX** - Exclusive word access
- **LDREXB/STREXB** - Exclusive byte access
- **LDREXH/STREXH** - Exclusive halfword access
- **CLREX** - Clear Exclusive
- Atomic operation sequences

### 21. System Register Access
- **MRS/MSR** - Access to special registers
- **MSP/PSP** - Main/Process Stack Pointers
- **PRIMASK/BASEPRI/FAULTMASK** - Interrupt control
- **CONTROL** - Control register access

### 22. Saturating Arithmetic
- **SSAT/USAT** - Signed/Unsigned Saturate
- Immediate and register-shifted saturate operations
- Overflow detection and clamping

### 23. Advanced Bit Manipulation
- **CLZ** - Count Leading Zeros
- **REV/REV16/REVSH** - Byte reverse operations
- **RBIT** - Bit reverse
- **BFI/BFC/UBFX/SBFX** - Advanced bitfield operations

### 24. Wait and Hint Instructions
- **WFI** - Wait For Interrupt
- **WFE** - Wait For Event
- **SEV** - Send Event
- **YIELD** - Yield processor hint

### 25. Debug Instructions
- **BKPT** - Breakpoint with immediate values
- **NOP/NOP.W** - No-operation instructions

### 26. Complex IT Blocks
- **ITTTE/ITTEE** - Complex multi-instruction blocks
- Nested condition testing
- Multiple condition codes in sequence

### 27. Performance Test Patterns
- Loop optimization patterns
- Branch prediction tests
- Complex arithmetic sequences
- Memory access patterns

## Test Data Organization

The test program includes comprehensive test data:

### Memory Arrays
- **test_array_32** - 32-bit word arrays for load/store testing
- **test_array_16** - 16-bit halfword arrays
- **test_array_8** - 8-bit byte arrays
- **bit_patterns** - Special bit patterns for manipulation tests

### Constants and Patterns
- **test_constants** - Various test values (positive, negative, boundary)
- **bit_patterns** - Patterns for bit manipulation verification
- **workspace** - Working memory for complex operations

### System Addresses
- NVIC register addresses for system testing
- SCB (System Control Block) addresses
- SysTick timer addresses
- Peripheral memory map constants

## Test Validation

Each test function includes comprehensive validation:
- Input/output verification
- Memory content checking
- Register state validation
- Condition flag verification
- Error code generation for failures

## Build and Execution

### Building
```bash
cd tests/assembly
make armv7m_test.hex    # Creates comprehensive test binary
make clean              # Clean build artifacts
```

### Execution
```bash
cd ../../
cmake --build build
./build/bin/arm_m_tlm tests/assembly/armv7m_test.hex
```

### Debug Mode
```bash
./build/bin/arm_m_tlm tests/assembly/armv7m_test.hex --debug
```

## Expected Behavior

When run successfully, the test should:
1. Execute each test section in sequence
2. Each section should return success code (0x99)  
3. Final success code should be 0x00
4. Program should end in infinite loop

Any failure will cause the program to return error code 0xBA and potentially branch to the infinite loop early.

## Troubleshooting

### Common Issues
1. **Undefined Instruction** - Check ARMv7-M support in simulator
2. **Memory Access Fault** - Verify memory map configuration
3. **Privilege Violation** - Check system register access permissions
4. **IT Block Violations** - Verify conditional execution implementation

### Debug Tips
1. Enable instruction tracing
2. Monitor PC progression through test sections
3. Check register values at test boundaries
4. Verify memory contents after load/store operations

## Compatibility

This test requires:
- ARMv7-M architecture (Cortex-M3 or higher)
- ARM Thumb-2 instruction set support  
- Hardware multiply/divide units
- System control registers (for MRS/MSR tests)
- Memory protection unit (for system register access)

The test will not work correctly on ARMv6-M (Cortex-M0/M0+) cores as many instructions are not available.

## Coverage Report

✅ **Complete Coverage:**
- All T16 basic instructions
- All T32 extended instructions
- All addressing modes
- All data sizes and signedness
- All conditional execution patterns
- All arithmetic and logical operations
- All memory barrier types
- All system register access
- All exclusive memory operations

🔍 **Advanced Features:**
- Complex IT block patterns
- Performance optimization patterns
- Debug and trace support
- Interrupt and exception handling preparation
- Cache and memory management hints
