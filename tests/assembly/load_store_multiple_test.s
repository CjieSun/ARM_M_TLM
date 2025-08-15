.syntax unified
.thumb
.text
.global _start

# ARMv6-M Thumb Load/Store Multiple Instructions Test
# This file tests LDM, STM, PUSH, and POP instructions

_start:
    # Initialize test data
    mov r0, #1
    mov r1, #2
    mov r2, #3
    mov r3, #4
    mov r4, #5
    mov r5, #6
    mov r6, #7
    mov r7, #8
    
    # Test PUSH instruction (Format 14)
    push {r0, r1, r2, r3}  # Push multiple registers
    
    # Modify registers to test POP
    mov r0, #0
    mov r1, #0
    mov r2, #0 
    mov r3, #0
    
    # Test POP instruction (Format 14)
    pop {r0, r1, r2, r3}   # Pop multiple registers
    
    # r0-r3 should now be restored to 1,2,3,4
    
    # Test PUSH with LR
    mov lr, #0xABCDEF00    # Set link register
    push {r4, r5, lr}      # Push registers including LR
    
    # Modify registers
    mov r4, #0
    mov r5, #0
    mov lr, #0
    
    # Test POP with PC (this will cause a branch)
    adr r6, after_pop_pc   # Set up expected return address
    mov lr, r6             # Store in LR first
    push {lr}              # Push return address
    pop {pc}               # Pop into PC (causes branch)
    
    # Should never reach here
    mov r0, #0xBAD
    b end_test
    
after_pop_pc:
    # We should arrive here after POP {PC}
    mov r8, #0x111         # Mark successful POP PC
    
    # Restore r4, r5, lr from earlier push
    pop {r4, r5, lr}       # Should restore to 5, 6, 0xABCDEF00
    
    # Test LDM/STM instructions (Format 15)
    adr r0, buffer         # Buffer address for LDM/STM test
    
    # Prepare test data
    mov r1, #0x10
    mov r2, #0x20  
    mov r3, #0x30
    mov r4, #0x40
    mov r5, #0x50
    
    # Test STM (Store Multiple)
    stm r0!, {r1, r2, r3, r4, r5}  # Store multiple with writeback
    
    # Clear registers to test LDM
    mov r1, #0
    mov r2, #0
    mov r3, #0
    mov r4, #0  
    mov r5, #0
    
    # Reset base address
    adr r0, buffer
    
    # Test LDM (Load Multiple)
    ldm r0!, {r1, r2, r3, r4, r5}  # Load multiple with writeback
    
    # r1-r5 should now be restored to 0x10, 0x20, 0x30, 0x40, 0x50
    
    # Test partial register lists
    adr r0, buffer
    mov r6, #0x60
    mov r7, #0x70
    
    # Store only specific registers
    stm r0!, {r1, r3, r6}  # Store r1, r3, r6 (non-consecutive)
    
    # Clear and reload
    mov r1, #0
    mov r3, #0
    mov r6, #0
    
    # Reset base address  
    adr r0, buffer
    ldm r0!, {r1, r3, r6}  # Load back same registers
    
    # Test PUSH/POP with larger register sets
    push {r0-r7}           # Push all low registers
    
    # Clear all registers
    mov r0, #0
    mov r1, #0
    mov r2, #0
    mov r3, #0
    mov r4, #0
    mov r5, #0
    mov r6, #0
    mov r7, #0
    
    # Restore all registers
    pop {r0-r7}            # Pop all low registers
    
    # Test empty register list edge case
    push {}                # Should be valid (no operation)
    pop {}                 # Should be valid (no operation)
    
    # Test single register PUSH/POP
    mov r2, #0xBEEF
    push {r2}              # Push single register
    mov r2, #0             # Clear register
    pop {r2}               # Pop single register
    
    # Test PUSH/POP with mixed registers
    mov r0, #0xDEAD
    mov r3, #0xBEEF
    mov r7, #0xCAFE
    
    push {r0, r3, r7}      # Push non-consecutive registers
    
    # Clear registers
    mov r0, #0
    mov r3, #0  
    mov r7, #0
    
    pop {r0, r3, r7}       # Pop back same registers
    
    # Verify stack pointer alignment
    mov r1, sp             # Get current stack pointer
    and r2, r1, #3         # Check if aligned to 4-byte boundary
    # r2 should be 0 if properly aligned
    
    # Test LDM/STM without writeback
    adr r0, buffer2
    mov r1, #0xAAAA
    mov r2, #0xBBBB
    
    stm r0, {r1, r2}       # Store without writeback
    # r0 should still point to buffer2
    
    mov r1, #0
    mov r2, #0
    ldm r0, {r1, r2}       # Load without writeback
    # r0 should still point to buffer2
    
    mov r9, #0x999         # Success marker
    b end_test

end_test:
    # Infinite loop
    b end_test

.align 2
buffer:
    .space 64              # Buffer for LDM/STM tests

buffer2:  
    .space 32              # Additional buffer