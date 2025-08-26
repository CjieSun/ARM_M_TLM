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
CFLAGS = -mcpu=cortex-m0plus -mthumb -Wall -Wextra -std=c99 -Os
ASFLAGS = -mcpu=cortex-m0plus -mthumb
LDFLAGS = -T linker_script.ld -nostdlib
EXPLDFLAGS = -T exception_link.ld -nostdlib

# Test files
# Working/fixed tests  
WORKING_TEST_SOURCES = data_processing_test.s load_store_test.s branch_test.s instruction_size_test.s bl_test.s

# Simplified working tests
SIMPLE_TEST_SOURCES = simple_miscellaneous_test.s simple_load_store_multiple_test.s simple_comprehensive_test.s

# Exception and NVIC tests
EXCEPTION_TEST_SOURCES = exception_test.s nvic_timer_test.s systick_test.s
EXCEPTION_TEST_OBJECTS = $(EXCEPTION_TEST_SOURCES:.s=.o)
EXCEPTION_ELFS = $(EXCEPTION_TEST_SOURCES:.s=.elf)

# All working tests
TEST_SOURCES = $(WORKING_TEST_SOURCES) $(SIMPLE_TEST_SOURCES)
C_TEST_SOURCES = c_test.c
C_TEST_OBJECTS = $(C_TEST_SOURCES:.c=.o)
C_TEST_ELFS = $(C_TEST_SOURCES:.c=.elf)

TEST_OBJECTS = $(TEST_SOURCES:.s=.o)
TEST_ELFS = $(TEST_SOURCES:.s=.elf)
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

# Link exception tests to ELF with exception linker script
$(EXCEPTION_ELFS): %.elf: %.o exception_link.ld
	$(LD) $(EXPLDFLAGS) -o $@ $<

# Link other objects to ELF with default linker script
%.elf: %.o linker_script.ld
	$(LD) $(LDFLAGS) -o $@ $<

# Convert ELF to Intel HEX
%.hex: %.elf %.lst
	$(OBJCOPY) -O ihex $< $@

# Generate disassembly listing
%.lst: %.elf
	$(OBJDUMP) -D -S $< > $@

# Run individual tests with the simulator
run-data-processing: data_processing_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug --trace

run-load-store: load_store_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug --trace

run-branch: branch_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug --trace

# Run BL test
run-bl: bl_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug --trace

# Run instruction size test
run-instr-size: instruction_size_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug --trace

run-multiple: simple_load_store_multiple_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug --trace

run-misc: simple_miscellaneous_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug --trace

run-comprehensive: simple_comprehensive_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug --trace

run-c-test: c_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug --trace

# Run exception tests
run-exception: exception_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug --trace

run-nvic-timer: nvic_timer_test.hex
	../../build/bin/arm_m_tlm --hex $< --debug --trace

# Run all tests
run-all: $(TEST_HEXS)
	@echo "Running all tests..."
	@for test in $(TEST_HEXS); do \
		echo "Running $$test..."; \
		../../build/bin/arm_m_tlm --hex $$test --debug; \
		echo ""; \
	done

# Generate comprehensive test reports
test-report: $(TEST_HEXS)
	@echo "Generating comprehensive test reports..."
	python3 ../../tools/test_runner.py \
		--simulator ../../build/bin/arm_m_tlm \
		--test-dir . \
		--output-dir ../../reports
	@echo "Test reports generated in ../../reports/"

# Generate test report with JSON only
test-report-json: $(TEST_HEXS)
	@echo "Generating JSON test report..."
	python3 ../../tools/test_runner.py \
		--simulator ../../build/bin/arm_m_tlm \
		--test-dir . \
		--output-dir ../../reports \
		--json
	@echo "JSON test report generated in ../../reports/"

# Generate test report with HTML only
test-report-html: $(TEST_HEXS)
	@echo "Generating HTML test report..."
	python3 ../../tools/test_runner.py \
		--simulator ../../build/bin/arm_m_tlm \
		--test-dir . \
		--output-dir ../../reports \
		--html
	@echo "HTML test report generated in ../../reports/"

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

.PHONY: all clean run-all doc check-toolchain test-report test-report-json test-report-html
.SECONDARY: $(TEST_OBJECTS) $(TEST_ELFS)