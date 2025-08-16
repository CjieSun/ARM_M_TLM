.syntax unified
.thumb
.text
.global _start

@ ARMv6-M Thumb Data Processing Instructions Test
@ This file tests all major data processing instruction formats

_start:
    @ Test Format 1: Move shifted register
    ldr r0, =0x12345678    @ Load test value
    lsls r1, r0, #4        @ Logical shift left by 4
    lsrs r2, r0, #8        @ Logical shift right by 8  
    asrs r3, r0, #12       @ Arithmetic shift right by 12
    
    @ Test Format 2: Add/subtract register/immediate
    movs r0, #10
    movs r1, #5
    adds r2, r0, r1        @ Add registers: r2 = r0 + r1
    subs r3, r0, r1        @ Subtract registers: r3 = r0 - r1
    adds r4, r0, #3        @ Add immediate: r4 = r0 + 3
    subs r5, r0, #2        @ Subtract immediate: r5 = r0 - 2
    
    @ Test Format 3: Move/compare/add/subtract immediate
    movs r0, #0xFF         @ Move immediate
    cmp r0, #0x80          @ Compare with immediate
    adds r0, #5            @ Add immediate to register
    subs r0, #10           @ Subtract immediate from register
    
    @ Test Format 4: ALU operations
    movs r0, #0x5A         @ Test pattern
    movs r1, #0x3C         @ Test pattern
    
    ands r2, r0            @ Bitwise AND (r2 = r0 & r1, but use r0)
    mov r2, r0             @ Copy for AND
    ands r2, r1            @ Bitwise AND
    eors r3, r0            @ Bitwise EOR (XOR) - need to set up first
    mov r3, r0             @ Copy for EOR
    eors r3, r1            @ Bitwise EOR
    orrs r4, r0            @ Bitwise OR - need to set up first  
    mov r4, r0             @ Copy for OR
    orrs r4, r1            @ Bitwise OR
    bics r5, r0            @ Bit clear (AND NOT) - need to set up first
    mov r5, r0             @ Copy for BIC
    bics r5, r1            @ Bit clear
    mvns r6, r1            @ Bitwise NOT
    
    @ Shift operations
    movs r7, #4
    lsls r0, r7            @ Logical shift left by r7
    lsrs r1, r7            @ Logical shift right by r7
    asrs r2, r7            @ Arithmetic shift right by r7
    
    @ Arithmetic operations
    movs r0, #15
    movs r1, #3
    muls r2, r0            @ Multiply: r2 = r0 * r1 (Thumb format)
    negs r3, r1            @ Negate: r3 = -r1
    
    @ Test operations
    tst r0, r1             @ Test (AND but don't store result)
    cmp r0, r1             @ Compare (SUB but don't store result)
    cmn r0, r1             @ Compare negative (ADD but don't store result)
    
    @ Test Format 5: Hi register operations
    movs r0, #0x10         @ Small immediate 
    lsls r0, r0, #8        @ Shift to make 0x1000
    mov r8, r0             @ Move to high register
    movs r1, #0x20         @ Small immediate   
    lsls r1, r1, #8        @ Shift to make 0x2000
    mov r9, r1             @ Move to high register
    add r8, r9             @ Add high registers
    cmp r8, r9             @ Compare high registers
    mov r0, r8             @ Move from high to low register
    mov r10, r0            @ Move from low to high register
    
    @ Test Format 12: Load address (ADD to PC/SP)
    adr r0, test_data      @ Load address relative to PC
    add r1, sp, #64        @ Add immediate to SP
    
    @ Infinite loop to end test
end_loop:
    b end_loop

.align 2
test_data:
    .word 0x12345678
    .word 0xABCDEF00
