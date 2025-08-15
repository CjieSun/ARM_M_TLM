.syntax unified
.thumb
.text
.global _start

# ARMv6-M Thumb Comprehensive Instruction Test
# This file tests a representative sample of all ARMv6-M Thumb instruction formats

_start:
    # Initialize stack pointer
    ldr sp, =stack_top
    
    # Test sequence: Data Processing -> Load/Store -> Branches -> Multiple -> Misc
    
    # === DATA PROCESSING TESTS ===
    bl test_data_processing
    
    # === LOAD/STORE TESTS ===
    bl test_load_store
    
    # === BRANCH TESTS ===  
    bl test_branches
    
    # === LOAD/STORE MULTIPLE TESTS ===
    bl test_multiple
    
    # === MISCELLANEOUS TESTS ===
    bl test_miscellaneous
    
    # All tests completed
    mov r0, #0x0000        # Success code
    b infinite_loop

# Data Processing Test Subroutine
test_data_processing:
    push {lr}
    
    # Format 1: Shifted registers
    mov r0, #0x12345678
    lsl r1, r0, #4         # Shift left
    lsr r2, r0, #8         # Shift right logical
    asr r3, r0, #12        # Shift right arithmetic
    
    # Format 2: Add/subtract 
    mov r4, #10
    mov r5, #3
    add r6, r4, r5         # Register add
    sub r7, r4, r5         # Register subtract
    add r8, r4, #2         # Immediate add
    sub r9, r4, #1         # Immediate subtract
    
    # Format 3: Immediate operations
    mov r0, #0xAA
    cmp r0, #0x80
    add r0, #5
    sub r0, #10
    
    # Format 4: ALU operations
    mov r1, #0x5A
    mov r2, #0x3C
    and r3, r1, r2         # AND
    eor r4, r1, r2         # XOR  
    orr r5, r1, r2         # OR
    bic r6, r1, r2         # Bit clear
    mvn r7, r2             # NOT
    
    # Format 5: Hi register ops
    mov r8, #0x1000
    add r8, r0             # Add to hi reg
    
    pop {pc}

# Load/Store Test Subroutine  
test_load_store:
    push {lr}
    
    # Set up test data
    adr r0, test_buffer
    mov r1, #0x12345678
    mov r2, #0xABCD
    
    # Format 6: Register offset
    mov r3, #4
    str r1, [r0, r3]       # Store with register offset
    ldr r4, [r0, r3]       # Load with register offset
    
    # Format 7: Sign-extended loads  
    mov r3, #8
    strh r2, [r0, r3]      # Store halfword
    ldrh r5, [r0, r3]      # Load halfword
    ldsh r6, [r0, r3]      # Load sign-extended halfword
    
    # Format 8: Halfword immediate
    strh r2, [r0, #12]     # Store halfword immediate
    ldrh r7, [r0, #12]     # Load halfword immediate
    
    # Format 9: Word immediate
    str r1, [r0, #16]      # Store word immediate  
    ldr r8, [r0, #16]      # Load word immediate
    
    # PC-relative load
    ldr r9, =const_data
    
    # Format 11: SP-relative
    str r1, [sp, #8]       # Store SP-relative
    ldr r10, [sp, #8]      # Load SP-relative
    
    pop {pc}

# Branch Test Subroutine
test_branches:
    push {lr}
    
    # Unconditional branch test
    b branch_target1
    mov r0, #0xBAD         # Should never execute
    
branch_target1:
    # Conditional branch tests
    mov r1, #5
    mov r2, #5
    cmp r1, r2             # Set Z flag
    beq branch_target2     # Should branch (equal)
    mov r0, #0xBAD         # Should never execute
    
branch_target2:
    mov r1, #10
    cmp r1, r2             # Clear Z flag  
    bne branch_target3     # Should branch (not equal)
    mov r0, #0xBAD         # Should never execute
    
branch_target3:
    # Test BL (branch with link)
    bl subroutine_test
    
    # Test BX (branch exchange)
    adr r3, bx_target + 1  # Thumb address
    bx r3
    
bx_target:
    mov r4, #0x123         # Mark BX success
    
    pop {pc}

subroutine_test:
    mov r5, #0xBEEF        # Mark subroutine called
    bx lr                  # Return

# Load/Store Multiple Test Subroutine
test_multiple:
    push {lr}
    
    # PUSH/POP test
    mov r0, #1
    mov r1, #2  
    mov r2, #3
    mov r3, #4
    
    push {r0-r3}           # Push multiple
    
    # Clear registers
    mov r0, #0
    mov r1, #0
    mov r2, #0
    mov r3, #0
    
    pop {r0-r3}            # Pop multiple
    
    # LDM/STM test
    adr r4, test_buffer
    mov r5, #0xAA
    mov r6, #0xBB
    
    stm r4!, {r5, r6}      # Store multiple
    
    mov r5, #0
    mov r6, #0
    adr r4, test_buffer
    
    ldm r4!, {r5, r6}      # Load multiple
    
    pop {pc}

# Miscellaneous Test Subroutine
test_miscellaneous:
    push {lr}
    
    # SP adjustment tests
    mov r0, sp             # Save SP
    
    add sp, #32            # Increase stack
    sub sp, #16            # Decrease stack  
    sub sp, #16            # Back to original
    
    # SVC test (software interrupt)
    svc #0x42              # Software interrupt
    
    pop {pc}

infinite_loop:
    b infinite_loop

.align 2
const_data:
    .word 0xDEADBEEF

test_buffer:
    .space 64

.align 2
    .space 512             # Stack space
stack_top: