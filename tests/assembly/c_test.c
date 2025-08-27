#include <stdint.h>
#include <stdio.h>

#define TRACE (*(volatile unsigned char *)0x40000000)

// Forward declarations for linker script symbols
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss;

void _init_data(void) {
    volatile uint32_t *src = &_sidata;
    volatile uint32_t *dst = &_sdata;

    // Copy data section from Flash to RAM
    while (dst < &_edata) {
        *dst++ = *src++;
    }

    // Zero out the BSS section
    dst = &_sbss;
    while (dst < &_ebss) {
        *dst++ = 0;
    }
}

int _read(int file, char* ptr, int len) {
    return 0;
}

int _close(int fd){
    return 0;
}

int _fstat_r(int fd) {
    return 0;
}

int _lseek_r(struct _reent *ptr, FILE *fp, long offset, int whence){
    return 0;
}

int _isatty_r(struct _reent *ptr, int fd) {
    return 0;
}

int _write(int file, const char *ptr, int len) {
  int x;

  for (x = 0; x < len; x++) {
    TRACE = *ptr++;
  }

  return (len);
}

// Implement puts for printf support
int puts(const char *s) {
    const char *p = s;
    while (*p) {
        TRACE = *p++;
    }
    TRACE = '\n';  // puts automatically adds newline
    return 1;
}

// Implement putchar for printf support
int putchar(int c) {
    TRACE = (char)c;
    return c;
}

// Simple C test to verify basic ARMv6-M instruction functionality
// This will be compiled to Thumb code and test the simulator

// Test data
volatile uint32_t test_data[16] = {
    0x12345678, 0xABCDEF00, 0x55AA55AA, 0xFF00FF00,
    0x00000001, 0x00000002, 0x00000004, 0x00000008,
    0x10101010, 0x20202020, 0x40404040, 0x80808080,
    0xDEADBEEF, 0xCAFEBABE, 0xFEEDFACE, 0xBADC0DE5
};

volatile uint32_t result_buffer[8];

// Simple arithmetic function (tests data processing)
uint32_t arithmetic_test(uint32_t a, uint32_t b) {
    uint32_t sum = a + b;           // ADD
    uint32_t diff = a - b;          // SUB  
    uint32_t product = sum & diff;  // AND
    uint32_t xor_result = sum ^ diff; // EOR
    return product | xor_result;    // ORR
}

// Memory access function (tests load/store)
void memory_test(void) {
    // Test different data sizes
    result_buffer[0] = test_data[0];                    // Word load/store
    result_buffer[1] = test_data[1] & 0xFFFF;          // Halfword mask
    result_buffer[2] = test_data[2] & 0xFF;            // Byte mask
    
    // Test array access
    for (int i = 0; i < 4; i++) {
        result_buffer[i + 3] = test_data[i + 4];
    }
}

// Branch and loop function (tests branches)  
uint32_t branch_test(uint32_t count) {
    uint32_t result = 0;
    
    // Test conditional branches in loop
    while (count > 0) {
        if (count & 1) {
            result += count;    // Add odd numbers
        } else {
            result -= count;    // Subtract even numbers  
        }
        count--;
    }
    
    return result;
}

// Stack usage function (tests PUSH/POP)
uint32_t stack_test(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    // Function with multiple parameters tests stack usage
    volatile uint32_t local_array[4];
    
    local_array[0] = a;
    local_array[1] = b;
    local_array[2] = c; 
    local_array[3] = d;
    
    // Return sum (tests local variable access)
    return local_array[0] + local_array[1] + local_array[2] + local_array[3];
}

// Main test function
int main(void) {
    //Test 1: Arithmetic operations
    uint32_t arith_result = arithmetic_test(0x12345678, 0xABCDEF00);
    result_buffer[7] = arith_result;

    printf("start\r\n");

    // Test 2: Memory operations
    memory_test();
    
    // Test 3: Branch operations
    uint32_t branch_result = branch_test(10);
    result_buffer[6] = branch_result;
    
    // Test 4: Stack operations  
    uint32_t stack_result = stack_test(
        test_data[0], test_data[1], test_data[2], test_data[3]
    );
    result_buffer[5] = stack_result;
    
    // Test 5: Bit manipulation
    uint32_t bits = test_data[8];
    bits <<= 2;                     // Left shift
    bits >>= 1;                     // Right shift
    bits &= 0x0F0F0F0F;            // Mask
    bits |= 0xF0F0F0F0;            // Set bits
    bits ^= 0xAAAAAAAA;            // Toggle bits
    result_buffer[4] = bits;
    
    // Success indicator
    result_buffer[0] = 0x600D;      // "GOOD" in hex
    
    printf("end\r\n");

    return 0;
}

// Entry point for bare-metal C code
void _start(void) {
    _init_data();
    main();
    // Infinite loop in case main returns
    while (1) {
        volatile int dummy = 0;
        (void)dummy;
    }
}