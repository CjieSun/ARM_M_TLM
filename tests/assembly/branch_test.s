.syntax unified
.thumb
.text
.global _start

@ ARMv6-M Thumb Branch Instructions Test
@ This file tests all major branch instruction formats

_start:
    @ Test unconditional branch (Format 18)
    b test_unconditional
    
    @ Should never reach here
    movs r0, #0xBA         @ Error code (use small immediate)
    
test_unconditional:
    movs r0, #1            @ Mark successful unconditional branch
    
    @ Test conditional branches (Format 16)
    movs r1, #10
    movs r2, #5
    
    @ Test EQ condition
    cmp r1, r2             @ r1 != r2, so Z flag clear
    beq fail_eq            @ Should not branch
    movs r3, #1            @ Mark EQ test passed
    b test_ne
    
fail_eq:
    movs r0, #0xEE         @ Error code for EQ failure (use small immediate)
    b end_test
    
test_ne:
    @ Test NE condition  
    cmp r1, r2             @ r1 != r2, so Z flag clear
    bne test_cmp_eq        @ Should branch
    movs r0, #0xEE         @ Error - should not reach here
    b end_test
    
test_cmp_eq:
    cmp r1, r1             @ r1 == r1, so Z flag set
    beq test_gt            @ Should branch
    movs r0, #0xEE         @ Error - should not reach here
    b end_test
    
test_gt:
    @ Test signed comparisons
    cmp r1, r2             @ r1 > r2 
    bgt test_lt            @ Should branch (10 > 5)
    movs r0, #0xEE         @ Error
    b end_test
    
test_lt:
    cmp r2, r1             @ r2 < r1
    blt test_ge            @ Should branch (5 < 10)
    movs r0, #0xEE         @ Error
    b end_test
    
test_ge:
    cmp r1, r2             @ r1 >= r2
    bge test_le            @ Should branch (10 >= 5)
    movs r0, #0xEE         @ Error  
    b end_test
    
test_le:
    cmp r2, r1             @ r2 <= r1
    ble test_unsigned      @ Should branch (5 <= 10)
    movs r0, #0xEE         @ Error
    b end_test
    
test_unsigned:
    @ Test unsigned comparisons
    ldr r4, =0xFFFFFFFF    @ Large unsigned value
    movs r5, #1
    
    cmp r4, r5             @ Unsigned: 0xFFFFFFFF > 1
    bhi test_ls            @ Should branch (higher)
    movs r0, #0xEE         @ Error
    b end_test
    
test_ls:
    cmp r5, r4             @ Unsigned: 1 < 0xFFFFFFFF  
    bls test_carry         @ Should branch (lower or same)
    movs r0, #0xEE         @ Error
    b end_test
    
test_carry:
    @ Test carry flag conditions
    ldr r1, =0xFFFFFFFF
    adds r1, #1            @ This will set carry flag
    bcs test_no_carry      @ Should branch (carry set)
    movs r0, #0xEE         @ Error
    b end_test
    
test_no_carry:
    movs r1, #5
    adds r1, #3            @ This won't set carry
    bcc test_negative      @ Should branch (carry clear)
    movs r0, #0xEE         @ Error
    b end_test
    
test_negative:
    @ Test sign flag conditions
    movs r1, #1
    subs r1, #2            @ Result = -1 (negative)
    bmi test_positive      @ Should branch (minus/negative)
    movs r0, #0xEE         @ Error
    b end_test
    
test_positive:
    movs r1, #5
    subs r1, #2            @ Result = 3 (positive)
    bpl test_overflow      @ Should branch (plus/positive)
    movs r0, #0xEE         @ Error
    b end_test
    
test_overflow:
    @ Test overflow conditions
    ldr r1, =0x7FFFFFFF    @ Largest positive 32-bit int
    adds r1, #1            @ This will cause overflow
    bvs test_bl            @ Should branch (overflow set)
    movs r0, #0xEE         @ Error
    b end_test
    
test_bl:
    @ Test branch with link (Format 19)
    movs r6, #0x12         @ Value to check after return
    bl subroutine          @ Call subroutine
    
    @ Check if we returned properly
    movs r0, #0x45         @ Expected value  
    cmp r6, r0             @ Should be modified by subroutine
    beq test_bx            @ If equal, subroutine worked
    movs r0, #0xBB         @ Error code for BL failure
    b end_test
    
test_bx:
    @ Test branch exchange (Format 5) - simplified
    adr r7, thumb_target   @ Address (assembler will set thumb bit)
    bx r7                  @ Branch to target
    
    @ Should never reach here
    movs r0, #0xBB
    b end_test
    
.align 2
thumb_target:
    movs r7, #0x12         @ Mark successful BX  
    
    @ Simplified test completion
    b test_complete
    
test_complete:
    movs r0, #0x99         @ Success code - all tests passed (use small immediate)
    b end_test

subroutine:
    @ Simple subroutine for BL test
    movs r6, #0x45         @ Modify the test value (use small immediate)
    bx lr                  @ Return to caller
    
end_test:
    @ Infinite loop 
    b end_test
