# Makefile for ARMv6-M Thumb Assembly Tests
# Compiles test assembly files to Intel HEX format for the ARM_M_TLM simulator

# Toolchain settings
TOOLCHAIN_PREFIX = arm-none-eabi-
CC = $(TOOLCHAIN_PREFIX)gcc
AS = $(TOOLCHAIN_PREFIX)as
LD = $(TOOLCHAIN_PREFIX)ld
OBJCOPY = $(TOOLCHAIN_PREFIX)objcopy
OBJDUMP = $(TOOLCHAIN_PREFIX)objdump

# Compiler flags for Cortex-M0 (ARMv6-M)
CFLAGS = -mcpu=cortex-m0 -mthumb -Wall -Wextra -std=c99 -Os
ASFLAGS = -mcpu=cortex-m0 -mthumb
LDFLAGS = -T linker_script.ld -nostdlib

# Test files
# Original complex tests (may have compilation issues)
ORIG_TEST_SOURCES = load_store_multiple_test.s miscellaneous_test.s comprehensive_test.s

# Working/fixed tests  
WORKING_TEST_SOURCES = data_processing_test.s load_store_test.s branch_test.s

# Simplified working tests
SIMPLE_TEST_SOURCES = simple_miscellaneous_test.s simple_load_store_multiple_test.s simple_comprehensive_test.s

# All working tests
TEST_SOURCES = $(WORKING_TEST_SOURCES) $(SIMPLE_TEST_SOURCES)
C_TEST_SOURCES = c_test.c

TEST_OBJECTS = $(TEST_SOURCES:.s=.o)
C_TEST_OBJECTS = $(C_TEST_SOURCES:.c=.o)
TEST_ELFS = $(TEST_SOURCES:.s=.elf)
C_TEST_ELFS = $(C_TEST_SOURCES:.c=.elf)
TEST_HEXS = $(TEST_SOURCES:.s=.hex) $(C_TEST_SOURCES:.c=.hex)
TEST_BINS = $(TEST_SOURCES:.s=.bin) $(C_TEST_SOURCES:.c=.bin)  
TEST_LISTINGS = $(TEST_SOURCES:.s=.lst) $(C_TEST_SOURCES:.c=.lst)

# Default target
all: $(TEST_HEXS) $(TEST_LISTINGS)

# Compile assembly to object
%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

# Compile C to object  
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Link object to ELF
%.elf: %.o linker_script.ld
	$(LD) $(LDFLAGS) -o $@ $<

# Convert ELF to Intel HEX
%.hex: %.elf
	$(OBJCOPY) -O ihex $< $@

# Convert ELF to binary  
%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

# Generate disassembly listing
%.lst: %.elf
	$(OBJDUMP) -D -S $< > $@

# Run individual tests with the simulator
run-data-processing: data_processing_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug

run-load-store: load_store_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug

run-branch: branch_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug

run-multiple: load_store_multiple_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug

run-misc: miscellaneous_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug

run-comprehensive: comprehensive_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug

run-c-test: c_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug

# Run all tests
run-all: $(TEST_HEXS)
	@echo "Running all tests..."
	@for test in $(TEST_HEXS); do \
		echo "Running $$test..."; \
		../../build/bin/arm_m_tlm --hex $$test --debug; \
		echo ""; \
	done

# Generate test documentation
doc: $(TEST_LISTINGS)
	@echo "Generated disassembly listings:"
	@ls -la *.lst

# Clean build artifacts
clean:
	rm -f *.o *.elf *.hex *.bin *.lst

# Check if toolchain is available
check-toolchain:
	@which $(CC) > /dev/null || (echo "ARM GCC toolchain not found. Please install arm-none-eabi-gcc"; exit 1)
	@echo "ARM GCC toolchain found: $$($(CC) --version | head -1)"

.PHONY: all clean run-all doc check-toolchain
.SECONDARY: $(TEST_OBJECTS) $(TEST_ELFS)