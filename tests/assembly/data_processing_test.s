.syntax unified
.thumb
.text
.global _start

# ARMv6-M Thumb Data Processing Instructions Test
# This file tests all major data processing instruction formats

_start:
    # Test Format 1: Move shifted register
    mov r0, #0x12345678    # Load test value
    lsl r1, r0, #4         # Logical shift left by 4
    lsr r2, r0, #8         # Logical shift right by 8  
    asr r3, r0, #12        # Arithmetic shift right by 12
    
    # Test Format 2: Add/subtract register/immediate
    mov r0, #10
    mov r1, #5
    add r2, r0, r1         # Add registers: r2 = r0 + r1
    sub r3, r0, r1         # Subtract registers: r3 = r0 - r1
    add r4, r0, #3         # Add immediate: r4 = r0 + 3
    sub r5, r0, #2         # Subtract immediate: r5 = r0 - 2
    
    # Test Format 3: Move/compare/add/subtract immediate
    mov r0, #0xFF          # Move immediate
    cmp r0, #0x80          # Compare with immediate
    add r0, #5             # Add immediate to register
    sub r0, #10            # Subtract immediate from register
    
    # Test Format 4: ALU operations
    mov r0, #0x5A          # Test pattern
    mov r1, #0x3C          # Test pattern
    
    and r2, r0, r1         # Bitwise AND
    eor r3, r0, r1         # Bitwise EOR (XOR)
    orr r4, r0, r1         # Bitwise OR
    bic r5, r0, r1         # Bit clear (AND NOT)
    mvn r6, r1             # Bitwise NOT
    
    # Shift operations
    mov r7, #4
    lsl r0, r7             # Logical shift left by r7
    lsr r1, r7             # Logical shift right by r7
    asr r2, r7             # Arithmetic shift right by r7
    
    # Arithmetic operations
    mov r0, #15
    mov r1, #3
    mul r2, r0, r1         # Multiply: r2 = r0 * r1
    neg r3, r1             # Negate: r3 = -r1
    
    # Test operations
    tst r0, r1             # Test (AND but don't store result)
    cmp r0, r1             # Compare (SUB but don't store result)
    cmn r0, r1             # Compare negative (ADD but don't store result)
    
    # Test Format 5: Hi register operations
    mov r8, #0x1000        # Move to high register
    mov r9, #0x2000        # Move to high register
    add r8, r9             # Add high registers
    cmp r8, r9             # Compare high registers
    mov r0, r8             # Move from high to low register
    mov r10, r0            # Move from low to high register
    
    # Test Format 12: Load address (ADD to PC/SP)
    adr r0, test_data      # Load address relative to PC
    add r1, sp, #64        # Add immediate to SP
    
    # Infinite loop to end test
end_loop:
    b end_loop

.align 2
test_data:
    .word 0x12345678
    .word 0xABCDEF00