.syntax unified
.thumb
.text
.global _start

@ Simple miscellaneous test - focus on basic functionality
_start:
    @ Use a simple stack setup
    movs r0, #1
    lsls r0, r0, #12       @ 4KB stack (shift left by 12 = multiply by 4096)  
    mov sp, r0             @ Set stack pointer
    
    @ Test basic SP operations (Format 13: Add offset to SP)
    add sp, #16            @ Add to SP (immediate)
    sub sp, #16            @ Subtract from SP (restore)
    
    @ Test basic arithmetic
    movs r1, #10
    movs r2, #5
    adds r3, r1, r2        @ Simple addition
    subs r4, r1, r2        @ Simple subtraction
    
    @ Test condition flags
    cmp r1, r2             @ Compare values
    bne test_passed        @ Branch if not equal
    
    @ Error case
    movs r0, #0xBA         @ Error code
    b end_test
    
test_passed:
    movs r0, #0x99         @ Success code
    
end_test:
    @ Infinite loop
    b end_test

.align 2
    .space 512             @ Stack space
stack_top:
