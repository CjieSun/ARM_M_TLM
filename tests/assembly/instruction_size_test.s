.syntax unified
.thumb
.text
.global _start

@ ARMv6-M Thumb Instruction Size Test
@ This file tests proper handling of both 16-bit and 32-bit Thumb instructions

_start:
    @ Initialize stack pointer  
    ldr r0, =stack_top
    mov sp, r0
    
    @ Test 16-bit instructions
    movs r0, #1          @ 16-bit: Move immediate (Format 3)
    movs r1, #2          @ 16-bit: Move immediate
    adds r2, r0, r1      @ 16-bit: Add register (Format 2)
    
    @ Test simple 16-bit load/store
    ldr r3, =test_data   @ 16-bit: PC-relative load (if offset small enough)
    str r2, [r3]         @ 16-bit: Store with zero offset
    
    @ Test 16-bit branches
    b test_16bit_complete @ 16-bit: Unconditional branch
    
test_16bit_complete:
    movs r4, #0x55       @ Mark 16-bit tests complete (small immediate)
    
    @ For ARMv6-M, most instructions are 16-bit
    @ 32-bit instructions are limited (BL, some loads with large offsets)
    
    @ Test BL instruction (this is 32-bit in Thumb mode)
    bl subroutine_test   @ This should be 32-bit: Branch with Link
    
    @ Test complete - set success indicator  
    movs r0, #99         @ Success code (small immediate)
    b infinite_loop

subroutine_test:
    movs r5, #0x77       @ Mark subroutine called
    bx lr                @ Return (16-bit)

test_data:
    .word 0xDEADBEEF

infinite_loop:
    b infinite_loop

.align 2
    .space 512           @ Stack space
stack_top: