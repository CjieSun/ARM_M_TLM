# ARM_M_TLM Test Reporting

This document describes the comprehensive test reporting system for the ARM_M_TLM simulator.

## Overview

The test reporting system runs all available tests and generates detailed reports showing:
- Individual test results with pass/fail status
- Performance metrics per test (instructions executed, memory operations, etc.)
- Overall summary statistics
- Execution times and system information

## Quick Start

### Option 1: One-Command Solution (Recommended)
```bash
./run_tests.sh
```
This script automatically:
1. Builds the simulator if needed
2. Builds all test files
3. Runs all tests and generates comprehensive reports

### Option 2: Manual Steps
```bash
# 1. Build simulator
mkdir build && cd build
cmake .. && make -j4
cd ..

# 2. Build test files
cd tests/assembly
make all
cd ../..

# 3. Generate test reports
python3 tools/test_runner.py \
    --simulator build/bin/arm_m_tlm \
    --test-dir tests/assembly \
    --output-dir reports
```

### Option 3: Using Makefile
```bash
cd tests/assembly
make test-report           # Generate HTML and JSON reports
make test-report-html      # Generate HTML report only
make test-report-json      # Generate JSON report only
```

## Generated Reports

### HTML Report
- Interactive web-based report with visual summaries
- Color-coded test results 
- Performance metrics in tabular format
- Located in: `reports/test_report_YYYYMMDD_HHMMSS.html`

### JSON Report
- Machine-readable format for CI/CD integration
- Complete test data including output/error logs
- Located in: `reports/test_report_YYYYMMDD_HHMMSS.json`

## Report Content

### Summary Statistics
- Total tests executed
- Pass/fail counts
- Success rate percentage
- Total execution time
- Total instructions executed across all tests
- Total memory operations

### Per-Test Metrics
- Execution duration
- Instructions executed
- Memory reads/writes
- Register reads/writes
- Branches taken
- IRQ count
- Instructions per second (performance metric)

## Test Suite

The current test suite includes:

1. **data_processing_test**: Tests arithmetic and logical operations
2. **load_store_test**: Tests memory load/store instructions
3. **branch_test**: Tests branch and jump instructions
4. **simple_miscellaneous_test**: Tests miscellaneous ARM instructions
5. **simple_load_store_multiple_test**: Tests multiple load/store operations
6. **simple_comprehensive_test**: Comprehensive test combining multiple instruction types
7. **c_test**: C language test compiled to ARM assembly

## Command Line Options

The test runner (`tools/test_runner.py`) supports these options:

```
python3 tools/test_runner.py [options]

Required:
  --simulator PATH     Path to ARM_M_TLM simulator executable
  --test-dir PATH      Path to test directory containing .hex files

Optional:
  --output-dir PATH    Output directory for reports (default: ./reports)
  --timeout SECONDS    Timeout per test in seconds (default: 60)
  --html              Generate HTML report only
  --json              Generate JSON report only
```

## Continuous Integration

For CI/CD pipelines, use the JSON output and check the exit code:

```bash
# Run tests and capture exit code
python3 tools/test_runner.py \
    --simulator build/bin/arm_m_tlm \
    --test-dir tests/assembly \
    --output-dir reports \
    --json

# Exit code 0 = all tests passed
# Exit code 1 = one or more tests failed/errored
echo "Test run exit code: $?"

# Parse JSON results
python3 -c "
import json
with open('reports/test_report_*.json') as f:
    data = json.load(f)
    summary = data['summary']
    print(f\"Success rate: {summary['success_rate']:.1f}%\")
    print(f\"Total tests: {summary['total_tests']}\")
    print(f\"Passed: {summary['passed']}\")
    print(f\"Failed: {summary['failed']}\")
"
```

## Adding New Tests

1. Create your test file (`.s` assembly or `.c` C source) in `tests/assembly/`
2. Add it to the appropriate variable in the Makefile:
   - `WORKING_TEST_SOURCES` for stable tests
   - `SIMPLE_TEST_SOURCES` for simplified tests
3. Run `make all` to build the new test
4. The test will automatically be included in future test runs

## Troubleshooting

### SystemC Library Not Found
```bash
sudo apt-get install libsystemc-dev
```

### ARM Toolchain Not Found
```bash
sudo apt-get install gcc-arm-none-eabi
```

### Test Fails to Load HEX File
- Ensure the .hex file exists in the test directory
- Check that the path is correct and accessible
- Verify the HEX file format is valid Intel HEX

### Python Dependencies
The test runner requires Python 3.6+ with standard library modules only (no external dependencies).

## File Structure

```
ARM_M_TLM/
├── run_tests.sh                    # One-command test runner
├── tools/
│   └── test_runner.py             # Main test runner script
├── tests/
│   └── assembly/
│       ├── Makefile               # Build and run individual tests
│       ├── *.s                    # Assembly test files
│       ├── *.c                    # C test files
│       └── *.hex                  # Compiled test binaries
└── reports/                       # Generated test reports
    ├── test_report_*.html
    └── test_report_*.json
```