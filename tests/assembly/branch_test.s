.syntax unified
.thumb
.text
.global _start

# ARMv6-M Thumb Branch Instructions Test
# This file tests all major branch instruction formats

_start:
    # Test unconditional branch (Format 18)
    b test_unconditional
    
    # Should never reach here
    mov r0, #0xBAD
    
test_unconditional:
    mov r0, #1             # Mark successful unconditional branch
    
    # Test conditional branches (Format 16)
    mov r1, #10
    mov r2, #5
    
    # Test EQ condition
    cmp r1, r2             # r1 != r2, so Z flag clear
    beq fail_eq            # Should not branch
    mov r3, #1             # Mark EQ test passed
    b test_ne
    
fail_eq:
    mov r0, #0xEEE         # Error code for EQ failure
    b end_test
    
test_ne:
    # Test NE condition  
    cmp r1, r2             # r1 != r2, so Z flag clear
    bne test_cmp_eq        # Should branch
    mov r0, #0xEEE         # Error - should not reach here
    b end_test
    
test_cmp_eq:
    cmp r1, r1             # r1 == r1, so Z flag set
    beq test_gt            # Should branch
    mov r0, #0xEEE         # Error - should not reach here
    b end_test
    
test_gt:
    # Test signed comparisons
    cmp r1, r2             # r1 > r2 
    bgt test_lt            # Should branch (10 > 5)
    mov r0, #0xEEE         # Error
    b end_test
    
test_lt:
    cmp r2, r1             # r2 < r1
    blt test_ge            # Should branch (5 < 10)
    mov r0, #0xEEE         # Error
    b end_test
    
test_ge:
    cmp r1, r2             # r1 >= r2
    bge test_le            # Should branch (10 >= 5)
    mov r0, #0xEEE         # Error  
    b end_test
    
test_le:
    cmp r2, r1             # r2 <= r1
    ble test_unsigned      # Should branch (5 <= 10)
    mov r0, #0xEEE         # Error
    b end_test
    
test_unsigned:
    # Test unsigned comparisons
    mov r4, #0xFFFFFFFF    # Large unsigned value
    mov r5, #1
    
    cmp r4, r5             # Unsigned: 0xFFFFFFFF > 1
    bhi test_ls            # Should branch (higher)
    mov r0, #0xEEE         # Error
    b end_test
    
test_ls:
    cmp r5, r4             # Unsigned: 1 < 0xFFFFFFFF  
    bls test_carry         # Should branch (lower or same)
    mov r0, #0xEEE         # Error
    b end_test
    
test_carry:
    # Test carry flag conditions
    mov r1, #0xFFFFFFFF
    adds r1, #1            # This will set carry flag
    bcs test_no_carry      # Should branch (carry set)
    mov r0, #0xEEE         # Error
    b end_test
    
test_no_carry:
    mov r1, #5
    adds r1, #3            # This won't set carry
    bcc test_negative      # Should branch (carry clear)
    mov r0, #0xEEE         # Error
    b end_test
    
test_negative:
    # Test sign flag conditions
    mov r1, #1
    subs r1, #2            # Result = -1 (negative)
    bmi test_positive      # Should branch (minus/negative)
    mov r0, #0xEEE         # Error
    b end_test
    
test_positive:
    mov r1, #5
    subs r1, #2            # Result = 3 (positive)
    bpl test_overflow      # Should branch (plus/positive)
    mov r0, #0xEEE         # Error
    b end_test
    
test_overflow:
    # Test overflow conditions
    mov r1, #0x7FFFFFFF    # Largest positive 32-bit int
    adds r1, #1            # This will cause overflow
    bvs test_bl            # Should branch (overflow set)
    mov r0, #0xEEE         # Error
    b end_test
    
test_bl:
    # Test branch with link (Format 19)
    mov r6, #0x123         # Value to check after return
    bl subroutine          # Call subroutine
    
    # Check if we returned properly
    cmp r6, #0x456         # Should be modified by subroutine
    beq test_bx            # If equal, subroutine worked
    mov r0, #0xBBB         # Error code for BL failure
    b end_test
    
test_bx:
    # Test branch exchange (Format 5)
    adr r7, thumb_target+1 # Address with thumb bit set
    bx r7                  # Branch to target
    
    # Should never reach here
    mov r0, #0xBBB
    b end_test
    
thumb_target:
    mov r7, #0x789         # Mark successful BX
    
    # Test BLX (branch with link and exchange)
    adr lr, return_point+1 # Set up return address
    adr r8, blx_target+1   # Target address
    blx r8                 # Branch with link and exchange
    
return_point:
    # Check if BLX worked
    cmp r8, #0xABC         # Should be modified by target
    beq test_complete      # Success!
    mov r0, #0xCCC         # Error code for BLX failure
    b end_test
    
blx_target:
    mov r8, #0xABC         # Modify register
    bx lr                  # Return using link register
    
test_complete:
    mov r0, #0x999         # Success code - all tests passed
    b end_test

subroutine:
    # Simple subroutine for BL test
    mov r6, #0x456         # Modify the test value
    bx lr                  # Return to caller
    
end_test:
    # Infinite loop 
    b end_test