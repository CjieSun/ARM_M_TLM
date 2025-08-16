.syntax unified
.thumb
.text
.global _start

@ ARMv6-M Thumb Miscellaneous Instructions Test
@ This file tests miscellaneous instructions including SP operations and SVC

_start:
@ Test Stack Pointer operations (Format 13)
    mov r0, sp             @ Save original SP
    
@ Test ADD SP, immediate
    add sp, #64            @ Add 64 to stack pointer
    mov r1, sp             @ Get new SP value
    sub r2, r1, r0         @ Calculate difference
@ r2 should equal 64
    
@ Test SUB SP, immediate  
    sub sp, #32            @ Subtract 32 from stack pointer
    mov r3, sp             @ Get new SP value
    sub r4, r1, r3         @ Calculate difference from previous
@ r4 should equal 32
    
@ Restore SP to original + 32 (net +32 from original)
    mov sp, r0             @ Restore original
    add sp, #32            @ Add 32 for testing space
    
@ Test SP alignment - ARM requires 4-byte alignment
    mov r5, sp
    and r6, r5, #3         @ Check alignment (should be 0)
    
@ Test larger SP adjustments
    sub sp, #256           @ Large decrement
    add sp, #256           @ Large increment (restore)
    
@ Test boundary values
    sub sp, #4             @ Minimum decrement
    add sp, #4             @ Minimum increment
    
@ Test maximum immediate value (508 bytes = 127 * 4)
    sub sp, #508           @ Maximum decrement
    add sp, #508           @ Maximum increment
    
@ Verify SP is word-aligned after operations
    mov r7, sp
    and r8, r7, #3         @ Should be 0 for proper alignment
    
@ Test Software Interrupt (Format 17)
@ Note: In a real system this would trigger an exception
@ For testing purposes, we just verify instruction decoding
    
@ Prepare for SVC test
    mov r0, #0x12          @ Test value to pass to SVC handler
    svc #0x55              @ Software interrupt with immediate 0x55
    
@ If SVC doesn't work, we continue here
    mov r1, #0x34          @ Mark that we returned from SVC
    
@ Test various SVC immediate values
    svc #0x00              @ Minimum SVC value
    svc #0xFF              @ Maximum SVC value  
    svc #0x80              @ Mid-range value
    
@ Test SP operations in combination with other instructions
    mov r2, #100
    push {r2}              @ This decrements SP by 4
    add sp, #8             @ Adjust SP past the pushed value
    sub sp, #4             @ Adjust back
    pop {r3}               @ This increments SP by 4, r3 should get r2's value
    
@ Test SP relative addressing combined with SP adjustment
    str r2, [sp, #4]       @ Store relative to SP
    add sp, #8             @ Adjust SP
    ldr r4, [sp, #-4]      @ Load with negative offset (should get r2)
    sub sp, #8             @ Restore SP
    
@ Test nested SP operations
    mov r5, sp             @ Save current SP
    sub sp, #16            @ Create stack frame
    
@ Use the stack frame
    str r0, [sp, #0]       @ Store at bottom of frame
    str r1, [sp, #4]       @ Store at offset 4
    str r2, [sp, #8]       @ Store at offset 8
    str r3, [sp, #12]      @ Store at offset 12
    
@ Load back in different registers
    ldr r6, [sp, #0]       @ Load from bottom
    ldr r7, [sp, #4]       @ Load from offset 4
    ldr r8, [sp, #8]       @ Load from offset 8
    ldr r9, [sp, #12]      @ Load from offset 12
    
@ Restore stack frame
    add sp, #16            @ Remove stack frame
    
@ Verify SP restoration
    cmp sp, r5             @ Compare with saved SP
    beq sp_test_passed     @ Branch if equal
    
@ SP test failed
    mov r10, #0xBAD        @ Error code
    b end_test
    
sp_test_passed:
    mov r10, #0x999        @ Success code
    
@ Test edge case: SP operations with register conflicts
    mov r11, sp            @ Save SP in r11
    mov r12, #24           @ Test immediate
    
@ These operations should work even though we're using SP
    sub sp, #12            @ Adjust SP
    str r11, [sp, #0]      @ Store original SP value
    add sp, #12            @ Restore SP
    
@ Final verification - SP should be back to original state
    cmp sp, r5             @ Compare with earlier saved value
    beq final_success      @ Branch if equal
    
    mov r10, #0xDEAD       @ Final error code
    b end_test
    
final_success:
    mov r10, #0xA11        @ All tests passed code

end_test:
@ Infinite loop
    b end_test
    
@ SVC Handler (simplified - in real system this would be in vector table)
svc_handler:
@ In a real system, this would be called by the SVC exception
@ For testing, this is just a placeholder
    mov r0, #0x5VC         @ Mark SVC was called
    bx lr                  @ Return (in real system would use different return)
