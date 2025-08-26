    .syntax unified
    .cpu cortex-m0plus
    .thumb

    .section .isr_vector
    .align 2
    .globl __Vectors
__Vectors:
    .long   __StackTop
    .long   Reset_Handler
    .long   NMI_Handler
    .long   HardFault_Handler
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   0
    .long   SVC_Handler
    .long   0
    .long   0
    .long   PendSV_Handler
    .long   SysTick_Handler
    .size __Vectors, . - __Vectors

    .text
    .thumb

    .thumb_func
Reset_Handler:
    ldr r0, =0x20002000
    mov sp, r0

    // Configure SysTick: LOAD=1000, VAL=0, CTRL=ENABLE|TICKINT|CLKSOURCE
    ldr r1, =0xE000E014 // LOAD
    movs r2, #100
    lsls r2, r2, #3     // 800
    adds r2, r2, #200   // 1000
    str r2, [r1]
    ldr r1, =0xE000E018 // VAL
    movs r2, #0
    str r2, [r1]
    ldr r1, =0xE000E010 // CTRL
    movs r2, #7         // ENABLE(1)|TICKINT(2)|CLKSOURCE(4)
    str r2, [r1]

    // Idle loop
1:
    b 1b

    .thumb_func
SysTick_Handler:
    // Indicate SysTick fired
    movs r0, #0xBB
    bx lr

    .thumb_func
NMI_Handler:        b .
    .thumb_func
HardFault_Handler:  b .
    .thumb_func
SVC_Handler:        bx lr
    .thumb_func
PendSV_Handler:     bx lr
