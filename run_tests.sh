#!/bin/bash
# Comprehensive test runner script for ARM_M_TLM
# This script builds the simulator, builds all tests, and generates comprehensive test reports

set -e

echo "ARM_M_TLM Comprehensive Test Runner"
echo "==================================="

# Check if we're in the right directory
if [ ! -f "CMakeLists.txt" ]; then
    echo "Error: This script must be run from the ARM_M_TLM root directory"
    exit 1
fi

# Build the simulator if needed
echo "Step 1: Building simulator..."
if [ ! -f "build/bin/arm_m_tlm" ]; then
    echo "  Creating build directory..."
    mkdir -p build
    cd build
    echo "  Running cmake..."
    cmake ..
    echo "  Building..."
    make -j4
    cd ..
else
    echo "  Simulator already built, skipping..."
fi

# Build test files
echo "Step 2: Building test files..."
cd tests/assembly
if ! command -v arm-none-eabi-gcc &> /dev/null; then
    echo "Error: ARM GCC toolchain not found. Please install arm-none-eabi-gcc"
    echo "Run: sudo apt-get install gcc-arm-none-eabi"
    exit 1
fi

echo "  Building test HEX files..."
make all > /dev/null 2>&1 || {
    echo "Error: Failed to build test files"
    exit 1
}
cd ../..

# Generate test reports
echo "Step 3: Running tests and generating reports..."
python3 tools/test_runner.py \
    --simulator build/bin/arm_m_tlm \
    --test-dir tests/assembly \
    --output-dir reports

echo ""
echo "Test execution completed!"
echo ""
echo "Reports generated in ./reports/ directory:"
ls -la reports/*.html reports/*.json 2>/dev/null | tail -2

echo ""
echo "To view the HTML report in your browser:"
echo "  firefox reports/test_report_*.html"
echo ""
echo "For CI/CD integration, use the JSON report:"
echo "  cat reports/test_report_*.json"