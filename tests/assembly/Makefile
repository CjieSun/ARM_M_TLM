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
# Include -g so ELF keeps symbols for GDB (the simulator consumes HEX separately)
CFLAGS = -mcpu=cortex-m4 -mthumb -Wall -Wextra -std=c99 -Os -g
ASFLAGS = -mcpu=cortex-m4 -mthumb -g
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
EXCEPTION_TEST_ELFS = $(EXCEPTION_TEST_SOURCES:.s=.elf)
EXCEPTION_TEST_HEXS = $(EXCEPTION_TEST_SOURCES:.s=.hex)
EXCEPTION_TEST_LISTINGS = $(EXCEPTION_TEST_SOURCES:.s=.lst)

# All working tests
TEST_SOURCES = $(WORKING_TEST_SOURCES) $(SIMPLE_TEST_SOURCES)
C_TEST_SOURCES = c_test.c
C_TEST_OBJECTS = $(C_TEST_SOURCES:.c=.o)
C_TEST_ELFS = $(C_TEST_SOURCES:.c=.elf)

# Combined lists
ALL_TEST_SOURCES = $(TEST_SOURCES) $(EXCEPTION_TEST_SOURCES)
ALL_TEST_OBJECTS = $(TEST_SOURCES:.s=.o) $(EXCEPTION_TEST_OBJECTS) $(C_TEST_OBJECTS)
ALL_TEST_ELFS = $(TEST_SOURCES:.s=.elf) $(EXCEPTION_TEST_ELFS) $(C_TEST_ELFS)
ALL_TEST_HEXS = $(TEST_SOURCES:.s=.hex) $(EXCEPTION_TEST_HEXS) $(C_TEST_SOURCES:.c=.hex)
ALL_TEST_LISTINGS = $(TEST_SOURCES:.s=.lst) $(EXCEPTION_TEST_LISTINGS) $(C_TEST_SOURCES:.c=.lst)

# Default target
all: $(ALL_TEST_HEXS) $(ALL_TEST_LISTINGS)

# Compile assembly to object
%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

# Compile C to object  
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Link exception tests to ELF with exception linker script
$(EXCEPTION_TEST_ELFS): %.elf: %.o exception_link.ld
	$(LD) $(EXPLDFLAGS) -o $@ $<

# Link other objects to ELF with default linker script
%.elf: %.o linker_script.ld
	$(LD) $(LDFLAGS) -o $@ $<

# Convert ELF to Intel HEX and generate listing
%.hex: %.elf
	$(OBJCOPY) -O ihex $< $@
	$(OBJDUMP) -D -S $< > $*.lst

# Launch gdb-multiarch against a built ELF. Usage: make gdb-systick_test
# Ensure the simulator is already running with --gdb/--gdb-port before invoking this.
GDB ?= gdb-multiarch
GDB_PORT ?= 3333

gdb-%: %.elf
	$(GDB) $< \
	  -ex "set architecture armv6-m" \
	  -ex "set disassemble-next-line on" \
	  -ex "set arm fallback-mode mthumb" \
	  -ex "target remote :$(GDB_PORT)"

# Simulator path
SIMULATOR = ../../build/bin/arm_m_tlm
SIM_FLAGS = --debug --trace

# Run individual tests with the simulator
run-%: %.hex
	$(SIMULATOR) --hex $< $(SIM_FLAGS)

# Run all tests
run-all: $(ALL_TEST_HEXS)
	@echo "Running all tests..."
	@for test in $(ALL_TEST_HEXS); do \
		echo "Running $$test..."; \
		$(SIMULATOR) --hex $$test --debug; \
		echo ""; \
	done

# Run only exception tests
run-exceptions: $(EXCEPTION_TEST_HEXS)
	@echo "Running exception tests..."
	@for test in $(EXCEPTION_TEST_HEXS); do \
		echo "Running $$test..."; \
		$(SIMULATOR) --hex $$test $(SIM_FLAGS); \
		echo ""; \
	done

# Generate comprehensive test reports
test-report: $(ALL_TEST_HEXS)
	@echo "Generating comprehensive test reports..."
	python3 ../../tools/test_runner.py \
		--simulator $(SIMULATOR) \
		--test-dir . \
		--output-dir ../../reports
	@echo "Test reports generated in ../../reports/"

# Generate test report with JSON only
test-report-json: $(ALL_TEST_HEXS)
	@echo "Generating JSON test report..."
	python3 ../../tools/test_runner.py \
		--simulator $(SIMULATOR) \
		--test-dir . \
		--output-dir ../../reports \
		--json
	@echo "JSON test report generated in ../../reports/"

# Generate test report with HTML only
test-report-html: $(ALL_TEST_HEXS)
	@echo "Generating HTML test report..."
	python3 ../../tools/test_runner.py \
		--simulator $(SIMULATOR) \
		--test-dir . \
		--output-dir ../../reports \
		--html
	@echo "HTML test report generated in ../../reports/"

# Generate test documentation
doc: $(ALL_TEST_LISTINGS)
	@echo "Generated disassembly listings:"
	@ls -la *.lst

# Show help
help:
	@echo "Available targets:"
	@echo "  all                 - Build all test files"
	@echo "  clean              - Clean all generated files"
	@echo "  doc                - Generate documentation listings"
	@echo "  help               - Show this help message"
	@echo ""
	@echo "Individual test runs:"
	@echo "  run-data-processing, run-load-store, run-branch, run-bl"
	@echo "  run-instr-size, run-multiple, run-misc, run-comprehensive"
	@echo "  run-c-test, run-exception, run-nvic-timer, run-systick"
	@echo ""
	@echo "Batch runs:"
	@echo "  run-all            - Run all tests"
	@echo "  run-exceptions     - Run only exception tests"
	@echo ""
	@echo "Test reports:"
	@echo "  test-report        - Generate HTML and JSON reports"
	@echo "  test-report-html   - Generate HTML report only"
	@echo "  test-report-json   - Generate JSON report only"

# Clean build artifacts
clean:
	rm -f *.o *.elf *.hex *.bin *.lst

# Check if toolchain is available
check-toolchain:
	@which $(CC) > /dev/null || (echo "ARM GCC toolchain not found. Please install arm-none-eabi-gcc"; exit 1)
	@echo "ARM GCC toolchain found: $$($(CC) --version | head -1)"

.PHONY: all clean run-all run-exceptions doc help check-toolchain test-report test-report-json test-report-html
