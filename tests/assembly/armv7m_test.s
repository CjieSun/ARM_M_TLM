.syntax unified
.thumb
.text
.global _start

@ ARMv7-M Thumb Instruction Set Test
@ This file tests ARMv7-M specific instructions including:
@ - IT (If-Then) blocks for conditional execution
@ - STRB/LDRB byte load/store operations  
@ - STMIA/LDMIA multiple load/store operations
@ - UMULL unsigned multiply long
@ - MLA multiply accumulate
@ - MLS multiply subtract

_start:
    @ Initialize stack pointer
    movs r0, #1
    lsls r0, r0, #12       @ 4KB stack
    mov sp, r0
    
    @ === IT BLOCK TESTS ===
    bl test_it_blocks
    
    @ === BYTE LOAD/STORE TESTS ===
    bl test_byte_operations
    
    @ === MULTIPLE LOAD/STORE TESTS ===
    bl test_multiple_operations
    
    @ === MULTIPLY INSTRUCTIONS TESTS ===
    bl test_multiply_operations
    
    @ All tests completed successfully
    movs r0, #0x00         @ Success code
    b infinite_loop

@ Test IT (If-Then) conditional execution blocks
test_it_blocks:
    push {lr}
    
    @ Test simple IT block - IT EQ (if equal)
    movs r0, #5
    movs r1, #5
    cmp r0, r1             @ Set flags: should be equal
    
    it eq                  @ If equal, execute next instruction
    moveq r2, #0xAA        @ Should execute (r0 == r1)
    
    @ Test IT block with different condition - IT NE (if not equal)
    movs r0, #5
    movs r1, #10
    cmp r0, r1             @ Set flags: should be not equal
    
    it ne                  @ If not equal, execute next instruction
    movne r3, #0xBB        @ Should execute (r0 != r1)
    
    @ Test 2-instruction IT block - ITE (If-Then-Else)
    movs r0, #8
    movs r1, #12
    cmp r0, r1             @ r0 < r1, so LT condition true
    
    ite lt                 @ If less than, then else
    movlt r4, #0xCC        @ Should execute (condition true)
    movge r5, #0xDD        @ Should NOT execute (condition false)
    
    @ Test 3-instruction IT block - ITEE (If-Then-Else-Else)
    movs r0, #15
    movs r1, #10  
    cmp r0, r1             @ r0 > r1, so GT condition true
    
    itee gt                @ If greater than, then else else
    movgt r6, #0xEE        @ Should execute (condition true)
    movle r7, #0xFF        @ Should NOT execute (condition false)
    movle r8, #0x11        @ Should NOT execute (condition false)
    
    pop {pc}

@ Test byte load/store operations (STRB/LDRB)
test_byte_operations:
    push {lr}
    
    @ Set up test buffer
    adr r0, test_buffer
    
    @ Test STRB (store byte) with immediate offset
    movs r1, #0x42         @ Test byte value
    strb r1, [r0, #0]      @ Store byte at offset 0
    strb r1, [r0, #1]      @ Store byte at offset 1
    strb r1, [r0, #4]      @ Store byte at offset 4
    
    @ Test STRB with register offset
    movs r2, #8            @ Offset register
    movs r3, #0x73         @ Different test value
    strb r3, [r0, r2]      @ Store byte at r0 + r2
    
    @ Test LDRB (load byte) with immediate offset
    ldrb r4, [r0, #0]      @ Load byte from offset 0
    ldrb r5, [r0, #1]      @ Load byte from offset 1 
    ldrb r6, [r0, #4]      @ Load byte from offset 4
    
    @ Test LDRB with register offset
    ldrb r7, [r0, r2]      @ Load byte from r0 + r2
    
    @ Verify values loaded match what was stored
    cmp r4, #0x42          @ Should match stored value
    bne byte_test_fail
    cmp r5, #0x42          @ Should match stored value
    bne byte_test_fail
    cmp r6, #0x42          @ Should match stored value  
    bne byte_test_fail
    cmp r7, #0x73          @ Should match stored value
    bne byte_test_fail
    
    @ Test passed
    movs r0, #0x99
    b byte_test_end
    
byte_test_fail:
    movs r0, #0xBA         @ Error code
    
byte_test_end:
    pop {pc}

@ Test multiple load/store operations (STMIA/LDMIA)  
test_multiple_operations:
    push {lr}
    
    @ Set up test data
    adr r0, test_buffer
    movs r1, #0x11         @ Test data
    movs r2, #0x22
    movs r3, #0x33
    movs r4, #0x44
    movs r5, #0x55
    
    @ Test STMIA (store multiple increment after)
    stmia r0!, {r1, r2, r3, r4, r5}  @ Store multiple with writeback
    
    @ Reset pointer and clear registers
    adr r0, test_buffer
    movs r1, #0
    movs r2, #0
    movs r3, #0
    movs r4, #0
    movs r5, #0
    
    @ Test LDMIA (load multiple increment after)
    ldmia r0!, {r1, r2, r3, r4, r5}  @ Load multiple with writeback
    
    @ Verify loaded values
    cmp r1, #0x11
    bne multiple_test_fail
    cmp r2, #0x22
    bne multiple_test_fail
    cmp r3, #0x33
    bne multiple_test_fail
    cmp r4, #0x44
    bne multiple_test_fail
    cmp r5, #0x55
    bne multiple_test_fail
    
    @ Test passed
    movs r0, #0x99
    b multiple_test_end
    
multiple_test_fail:
    movs r0, #0xBA         @ Error code
    
multiple_test_end:
    pop {pc}

@ Test multiply instructions (UMULL, MLA, MLS)
test_multiply_operations:
    push {lr}
    
    @ Test UMULL (unsigned multiply long)
    @ UMULL RdLo, RdHi, Rn, Rm - RdHi:RdLo = Rn * Rm (64-bit result)
    movs r0, #0x1000       @ 4096
    movs r1, #0x2000       @ 8192  
    umull r2, r3, r0, r1   @ r3:r2 = r0 * r1 = 0x02000000 (33554432)
    
    @ Verify result: r2 should contain low 32 bits, r3 should contain high 32 bits
    ldr r4, =0x02000000    @ Expected low part
    cmp r2, r4
    bne multiply_test_fail
    cmp r3, #0             @ Expected high part (should be 0 for this small result)
    bne multiply_test_fail
    
    @ Test MLA (multiply accumulate)
    @ MLA Rd, Rn, Rm, Ra - Rd = (Rn * Rm) + Ra
    movs r0, #7            @ Multiplicand
    movs r1, #8            @ Multiplier
    movs r2, #5            @ Addend
    mla r3, r0, r1, r2     @ r3 = (7 * 8) + 5 = 56 + 5 = 61
    
    cmp r3, #61
    bne multiply_test_fail
    
    @ Test MLS (multiply subtract)  
    @ MLS Rd, Rn, Rm, Ra - Rd = Ra - (Rn * Rm)
    movs r0, #6            @ Multiplicand
    movs r1, #4            @ Multiplier
    movs r2, #30           @ Minuend
    mls r3, r0, r1, r2     @ r3 = 30 - (6 * 4) = 30 - 24 = 6
    
    cmp r3, #6
    bne multiply_test_fail
    
    @ Test basic MUL (16-bit multiply) for comparison
    movs r0, #9
    movs r1, #7
    mul r2, r0, r1         @ r2 = r0 * r1 = 63
    
    cmp r2, #63
    bne multiply_test_fail
    
    @ All multiply tests passed
    movs r0, #0x99
    b multiply_test_end
    
multiply_test_fail:
    movs r0, #0xBA         @ Error code
    
multiply_test_end:
    pop {pc}

@ Infinite loop for test completion
infinite_loop:
    b infinite_loop

.align 2
test_buffer:
    .space 64              @ Test data buffer
