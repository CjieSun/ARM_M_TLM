#!/bin/bash
# Build script for ARM M-series TLM simulator with different core configurations

# Function to build for a specific core type
build_core() {
    local core_type=$1
    local build_dir="build_${core_type,,}"  # lowercase
    
    echo "Building for $core_type..."
    
    # Create build directory
    mkdir -p $build_dir
    cd $build_dir
    
    # Configure with specific core type
    cmake .. -DARM_CORE_TYPE=$core_type
    
    # Build
    make -j$(nproc)
    
    cd ..
    
    echo "Build for $core_type completed in $build_dir/"
}

# Parse command line arguments
if [ $# -eq 0 ]; then
    echo "Usage: $0 <core_type> [additional_cores...]"
    echo "Available core types:"
    echo "  CORTEX_M0      - ARM Cortex-M0 (ARMv6-M)"
    echo "  CORTEX_M0_PLUS - ARM Cortex-M0+ (ARMv6-M)"
    echo "  CORTEX_M3      - ARM Cortex-M3 (ARMv7-M)"
    echo "  CORTEX_M4      - ARM Cortex-M4 (ARMv7E-M)"
    echo "  CORTEX_M7      - ARM Cortex-M7 (ARMv7E-M)"
    echo "  CORTEX_M33     - ARM Cortex-M33 (ARMv8-M)"
    echo "  CORTEX_M55     - ARM Cortex-M55 (ARMv8-M)"
    echo ""
    echo "Examples:"
    echo "  $0 CORTEX_M0_PLUS"
    echo "  $0 CORTEX_M3 CORTEX_M4"
    echo "  $0 all    # Build all variants"
    exit 1
fi

# Handle special "all" case
if [ "$1" = "all" ]; then
    cores=("CORTEX_M0" "CORTEX_M0_PLUS" "CORTEX_M3" "CORTEX_M4" "CORTEX_M7" "CORTEX_M33" "CORTEX_M55")
else
    cores=("$@")
fi

# Build for each specified core
for core in "${cores[@]}"; do
    build_core $core
done

echo "All builds completed!"
