.syntax unified
.thumb
.text
.global _start

@ Simple load/store multiple test  
_start:
    @ Set up some test data
    adr r0, test_buffer    @ Base address
    movs r1, #0x11         @ Test values
    movs r2, #0x22
    movs r3, #0x33
    movs r4, #0x44
    
    @ Test store multiple (STM)
    stm r0!, {r1, r2, r3, r4}  @ Store multiple with writeback
    
    @ Reset pointer
    adr r0, test_buffer
    
    @ Clear registers
    movs r1, #0
    movs r2, #0
    movs r3, #0
    movs r4, #0
    
    @ Test load multiple (LDM)
    ldm r0!, {r1, r2, r3, r4}  @ Load multiple with writeback
    
    @ Simple verification - check if first value is correct
    cmp r1, #0x11
    beq test_passed
    
    @ Error case
    movs r0, #0xBA         @ Error code
    b end_test
    
test_passed:
    movs r0, #0x99         @ Success code
    
end_test:
    @ Infinite loop
    b end_test

.align 2
test_buffer:
    .space 64              @ Buffer for test data
