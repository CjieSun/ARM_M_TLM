#ifndef ARM_CORTEXM_CONFIG_H
#define ARM_CORTEXM_CONFIG_H

// ARM Cortex-M Architecture Version Control
// Define which ARM architecture version to target

// Architecture versions
#define ARMV6_M     1
#define ARMV7_M     2
#define ARMV7E_M    3
#define ARMV8_M     4

// Core type definitions
#define CORTEX_M0       1
#define CORTEX_M0_PLUS  2
#define CORTEX_M3       3
#define CORTEX_M4       4
#define CORTEX_M7       5
#define CORTEX_M33      6
#define CORTEX_M55      7

// Configuration: Choose your target core and architecture
#ifndef ARM_CORE_TYPE
#define ARM_CORE_TYPE CORTEX_M0_PLUS
#endif

// Automatically set architecture based on core type
#if ARM_CORE_TYPE == CORTEX_M0 || ARM_CORE_TYPE == CORTEX_M0_PLUS
    #define ARM_ARCH_VERSION ARMV6_M
    #define SUPPORTS_ARMV6_M 1
    #define SUPPORTS_ARMV7_M 0
    #define SUPPORTS_ARMV7E_M 0
    #define SUPPORTS_ARMV8_M 0
#elif ARM_CORE_TYPE == CORTEX_M3
    #define ARM_ARCH_VERSION ARMV7_M
    #define SUPPORTS_ARMV6_M 1
    #define SUPPORTS_ARMV7_M 1
    #define SUPPORTS_ARMV7E_M 0
    #define SUPPORTS_ARMV8_M 0
#elif ARM_CORE_TYPE == CORTEX_M4 || ARM_CORE_TYPE == CORTEX_M7
    #define ARM_ARCH_VERSION ARMV7E_M
    #define SUPPORTS_ARMV6_M 1
    #define SUPPORTS_ARMV7_M 1
    #define SUPPORTS_ARMV7E_M 1
    #define SUPPORTS_ARMV8_M 0
#elif ARM_CORE_TYPE == CORTEX_M33 || ARM_CORE_TYPE == CORTEX_M55
    #define ARM_ARCH_VERSION ARMV8_M
    #define SUPPORTS_ARMV6_M 1
    #define SUPPORTS_ARMV7_M 1
    #define SUPPORTS_ARMV7E_M 1
    #define SUPPORTS_ARMV8_M 1
#else
    #error "Unsupported ARM core type"
#endif

// Feature flags based on architecture version
#if SUPPORTS_ARMV6_M
    #define HAS_T16_BASIC_INSTRUCTIONS 1
    #define HAS_T32_BL 1
    #define HAS_MEMORY_BARRIERS 1
    #define HAS_SYSTEM_REGISTERS 1
#else
    #define HAS_T16_BASIC_INSTRUCTIONS 0
    #define HAS_T32_BL 0
    #define HAS_MEMORY_BARRIERS 0
    #define HAS_SYSTEM_REGISTERS 0
#endif

#if SUPPORTS_ARMV7_M
    #define HAS_T32_EXTENDED_INSTRUCTIONS 1
    #define HAS_ADVANCED_SIMD 0  // Only in some variants
    #define HAS_HARDWARE_DIVIDE 1
    #define HAS_SATURATING_ARITHMETIC 1
    #define HAS_BITFIELD_INSTRUCTIONS 1
    #define HAS_TABLE_BRANCH 1
#else
    #define HAS_T32_EXTENDED_INSTRUCTIONS 0
    #define HAS_ADVANCED_SIMD 0
    #define HAS_HARDWARE_DIVIDE 0
    #define HAS_SATURATING_ARITHMETIC 0
    #define HAS_BITFIELD_INSTRUCTIONS 0
    #define HAS_TABLE_BRANCH 0
#endif

#if SUPPORTS_ARMV7E_M
    #define HAS_DSP_EXTENSIONS 1
    #define HAS_FLOATING_POINT 1  // Optional
    #define HAS_SIMD_INSTRUCTIONS 1
#else
    #define HAS_DSP_EXTENSIONS 0
    #define HAS_FLOATING_POINT 0
    #define HAS_SIMD_INSTRUCTIONS 0
#endif

#if SUPPORTS_ARMV8_M
    #define HAS_SECURITY_EXTENSIONS 1
    #define HAS_TRUSTZONE 1
    #define HAS_STACK_LIMIT_CHECKING 1
    #define HAS_POINTER_AUTHENTICATION 1  // Optional
#else
    #define HAS_SECURITY_EXTENSIONS 0
    #define HAS_TRUSTZONE 0
    #define HAS_STACK_LIMIT_CHECKING 0
    #define HAS_POINTER_AUTHENTICATION 0
#endif

// Core-specific features
#if ARM_CORE_TYPE == CORTEX_M0
    #define HAS_BLX_REGISTER 0  // M0 doesn't have BLX
    #define MAX_INTERRUPTS 32
    #define HAS_MPU 0
#elif ARM_CORE_TYPE == CORTEX_M0_PLUS
    #define HAS_BLX_REGISTER 0  // M0+ doesn't have BLX either
    #define MAX_INTERRUPTS 32
    #define HAS_MPU 1  // Optional
#elif ARM_CORE_TYPE == CORTEX_M3
    #define HAS_BLX_REGISTER 1
    #define MAX_INTERRUPTS 240
    #define HAS_MPU 1  // Optional
#else
    #define HAS_BLX_REGISTER 1
    #define MAX_INTERRUPTS 240
    #define HAS_MPU 1
#endif

// Debug macros for build-time information
#if ARM_CORE_TYPE == CORTEX_M0
    #define ARM_CORE_NAME "Cortex-M0"
#elif ARM_CORE_TYPE == CORTEX_M0_PLUS
    #define ARM_CORE_NAME "Cortex-M0+"
#elif ARM_CORE_TYPE == CORTEX_M3
    #define ARM_CORE_NAME "Cortex-M3"
#elif ARM_CORE_TYPE == CORTEX_M4
    #define ARM_CORE_NAME "Cortex-M4"
#elif ARM_CORE_TYPE == CORTEX_M7
    #define ARM_CORE_NAME "Cortex-M7"
#elif ARM_CORE_TYPE == CORTEX_M33
    #define ARM_CORE_NAME "Cortex-M33"
#elif ARM_CORE_TYPE == CORTEX_M55
    #define ARM_CORE_NAME "Cortex-M55"
#endif

#if ARM_ARCH_VERSION == ARMV6_M
    #define ARM_ARCH_NAME "ARMv6-M"
#elif ARM_ARCH_VERSION == ARMV7_M
    #define ARM_ARCH_NAME "ARMv7-M"
#elif ARM_ARCH_VERSION == ARMV7E_M
    #define ARM_ARCH_NAME "ARMv7E-M"
#elif ARM_ARCH_VERSION == ARMV8_M
    #define ARM_ARCH_NAME "ARMv8-M"
#endif

#endif // ARM_CORTEXM_CONFIG_H
