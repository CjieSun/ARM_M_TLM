.syntax unified
.thumb
.text
.global _start

@ Minimal BL test to validate 32-bit decode and LR writeback

_start:
    ldr r0, =0x12345678   @ Some value
    bl target
    @ After return, LR should have return address, r1 set by callee
    movs r2, #0x45
    cmp r1, r2
    beq ok
    movs r0, #1           @ error code
    b end
ok:
    movs r0, #0           @ success
end:
    b end

.align 2
target:
    movs r1, #0x45
    bx lr
