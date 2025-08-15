# ARMv6-M Thumb Instruction Set Tests

This directory contains comprehensive assembly language tests for the ARMv6-M Thumb instruction set implementation in the ARM_M_TLM simulator.

## Test Files

### Individual Instruction Category Tests

1. **`data_processing_test.s`** - Tests all data processing instruction formats:
   - Format 1: Move shifted register (LSL, LSR, ASR)
   - Format 2: Add/subtract register/immediate
   - Format 3: Move/compare/add/subtract immediate  
   - Format 4: ALU operations (AND, EOR, LSL, LSR, ASR, ADC, SBC, ROR, TST, NEG, CMP, CMN, ORR, MUL, BIC, MVN)
   - Format 5: Hi register operations/branch exchange
   - Format 12: Load address (ADD to PC/SP)

2. **`load_store_test.s`** - Tests all load/store instruction formats:
   - Format 6: Load/store with register offset
   - Format 7: Load/store sign-extended byte/halfword (LDSB, LDSH, STRH)
   - Format 8: Load/store halfword with immediate
   - Format 9: Load/store with immediate offset
   - Format 11: SP-relative load/store
   - PC-relative loads
   - Different data sizes (byte, halfword, word)
   - Sign extension testing

3. **`branch_test.s`** - Tests all branch instruction formats:
   - Format 16: Conditional branch (all condition codes)
   - Format 17: Software interrupt (SVC)
   - Format 18: Unconditional branch
   - Format 19: Long branch with link (BL)
   - BX/BLX instructions
   - Condition code testing (EQ, NE, GT, LT, GE, LE, HI, LS, CS, CC, MI, PL, VS, VC)

4. **`load_store_multiple_test.s`** - Tests multiple transfer instructions:
   - Format 14: Push/pop registers (with LR/PC)
   - Format 15: Multiple load/store (LDM/STM)
   - Stack operations
   - Register list variations
   - Writeback modes

5. **`miscellaneous_test.s`** - Tests miscellaneous instructions:
   - Format 13: Add offset to Stack Pointer
   - SVC instruction testing
   - Stack alignment verification
   - Boundary condition testing

6. **`comprehensive_test.s`** - Integrated test combining all instruction types:
   - Sequential testing of all major instruction categories
   - Subroutine-based test structure
   - Comprehensive instruction interaction testing

## Building and Running Tests

### Prerequisites

1. ARM GCC toolchain (`arm-none-eabi-gcc`)
2. Built ARM_M_TLM simulator in `../../build/bin/arm_m_tlm`

### Build Commands

```bash
# Check if toolchain is available
make check-toolchain

# Build all tests
make all

# Build individual test
make data_processing_test.hex

# Clean build artifacts
make clean
```

### Running Tests

```bash
# Run individual tests
make run-data-processing
make run-load-store  
make run-branch
make run-multiple
make run-misc
make run-comprehensive

# Run all tests
make run-all

# Generate documentation
make doc
```

## Test Structure

Each test file follows this structure:

1. **Setup**: Initialize registers and test data
2. **Test Execution**: Exercise specific instruction formats
3. **Verification**: Check results and set status codes
4. **Infinite Loop**: End with controlled infinite loop

## Expected Results

- **Success codes**: Tests use specific values (like 0x999, 0xA11) to indicate success
- **Error codes**: Tests use specific values (like 0xBAD, 0xEEE) to indicate failures
- **Instruction tracing**: Use `--debug` flag to see detailed instruction execution

## Instruction Format Reference

The tests are organized by ARMv6-M Thumb instruction formats:

- **Formats 1-5**: Data processing and register operations
- **Formats 6-12**: Load/store operations  
- **Formats 13-14**: Stack and multiple transfer operations
- **Formats 15-19**: Multiple transfer and branch operations

## Debugging

1. **Disassembly**: Check `.lst` files for instruction encoding verification
2. **Simulator logs**: Use `--debug` flag for detailed execution tracing
3. **Register dumps**: Monitor register values during execution
4. **Memory dumps**: Verify load/store operations

## Test Coverage

These tests provide comprehensive coverage of:

- All 16-bit Thumb instruction encodings supported by ARMv6-M
- Edge cases and boundary conditions
- Instruction interactions and sequencing
- Condition code flag behavior
- Memory addressing modes
- Stack operations and alignment
- Branch and subroutine mechanics

## References

- ARMv6-M Architecture Reference Manual
- ARM Thumb Instruction Set Quick Reference