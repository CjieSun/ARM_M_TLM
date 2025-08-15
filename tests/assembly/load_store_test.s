.syntax unified
.thumb
.text
.global _start

# ARMv6-M Thumb Load/Store Instructions Test
# This file tests all major load/store instruction formats

_start:
    # Set up base addresses and test data
    adr r0, test_data      # Base address for test data
    mov r1, #0x12345678    # Test pattern
    mov r2, #0xABCD        # Test halfword
    mov r3, #0x5A          # Test byte
    
    # Test Format 6: Load/store with register offset
    mov r4, #4             # Offset register
    str r1, [r0, r4]       # Store word with register offset
    ldr r5, [r0, r4]       # Load word with register offset
    
    mov r4, #8
    strb r3, [r0, r4]      # Store byte with register offset
    ldrb r6, [r0, r4]      # Load byte with register offset
    
    # Test Format 7: Load/store sign-extended byte/halfword
    mov r4, #12
    strh r2, [r0, r4]      # Store halfword
    ldrh r5, [r0, r4]      # Load halfword
    ldsh r6, [r0, r4]      # Load sign-extended halfword
    
    mov r7, #0x80          # Negative byte value
    strb r7, [r0, #14]     # Store signed byte
    ldsb r8, [r0, #14]     # Load sign-extended byte
    
    # Test Format 8: Load/store halfword with immediate
    strh r2, [r0, #16]     # Store halfword immediate offset
    ldrh r5, [r0, #16]     # Load halfword immediate offset
    
    # Test Format 9: Load/store with immediate offset
    str r1, [r0, #20]      # Store word immediate offset
    ldr r6, [r0, #20]      # Load word immediate offset
    
    strb r3, [r0, #24]     # Store byte immediate offset  
    ldrb r7, [r0, #24]     # Load byte immediate offset
    
    # Test PC-relative load (Format 9 variant)
    ldr r8, =const_value   # Load constant using PC-relative addressing
    ldr r9, const_value    # Direct PC-relative load
    
    # Test Format 11: SP-relative load/store
    str r1, [sp, #4]       # Store to stack with offset
    ldr r10, [sp, #4]      # Load from stack with offset
    
    # Test various addressing modes with different data sizes
    mov r0, #0xFF000000    # Test sign extension
    adr r1, buffer
    
    # Store different data sizes
    str r0, [r1, #0]       # Store word
    strh r0, [r1, #4]      # Store halfword (low 16 bits)
    strb r0, [r1, #6]      # Store byte (low 8 bits)
    
    # Load back and check sign extension
    ldr r2, [r1, #0]       # Load word
    ldrh r3, [r1, #4]      # Load halfword (zero-extended)
    ldsh r4, [r1, #4]      # Load halfword (sign-extended)  
    ldrb r5, [r1, #6]      # Load byte (zero-extended)
    ldsb r6, [r1, #6]      # Load byte (sign-extended)
    
    # Test boundary conditions
    mov r7, #0
    strb r7, [r1, #7]      # Store zero byte
    ldrb r8, [r1, #7]      # Load zero byte
    
    mov r7, #0x8000
    strh r7, [r1, #8]      # Store negative halfword
    ldsh r9, [r1, #8]      # Load sign-extended (should be 0xFFFF8000)
    
    # Infinite loop to end test
end_loop:
    b end_loop

.align 2
const_value:
    .word 0xDEADBEEF

test_data:
    .space 64              # Reserve 64 bytes for test data

buffer:
    .space 32              # Reserve 32 bytes for buffer