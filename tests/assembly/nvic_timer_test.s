.syntax unified
.thumb
.text
.global _start

@ NVIC and Timer integration test
_start:
    @ Initialize stack pointer to top of RAM
    ldr r0, =0x20010000
    mov sp, r0
    
    @ Test NVIC register access
    bl test_nvic_registers
    
    @ Test timer interrupt generation
    bl test_timer_integration
    
    @ Success
    movs r0, #0x00         @ Success code
    b infinite_loop

@ Test NVIC register access
test_nvic_registers:
    push {lr}
    
    @ Test NVIC ISER (Interrupt Set Enable Register) 
    ldr r0, =0xE000E100    @ NVIC_ISER address
    movs r1, #0x01         @ Enable IRQ0
    str r1, [r0]
    
    @ Test NVIC ISPR (Interrupt Set Pending Register)
    ldr r0, =0xE000E200    @ NVIC_ISPR address  
    movs r1, #0x01         @ Set IRQ0 pending
    str r1, [r0]
    
    @ Read back to verify
    ldr r2, [r0]
    cmp r1, r2
    bne test_fail
    
    pop {pc}

@ Test Timer integration with interrupts
test_timer_integration:
    push {lr}
    
    @ Configure timer to generate interrupt
    ldr r0, =0x40004000    @ Timer base address
    movs r1, #100          @ Set timer compare value (low)
    str r1, [r0, #8]       @ MTIMECMP_LOW
    
    movs r1, #0            @ Clear high part
    str r1, [r0, #12]      @ MTIMECMP_HIGH
    
    @ Start timer by setting current time to 0
    movs r1, #0
    str r1, [r0]           @ MTIME_LOW
    str r1, [r0, #4]       @ MTIME_HIGH
    
    @ Wait for interrupt to fire
    @ In real system, timer would increment and trigger interrupt
    ldr r2, =1000          @ Wait counter (use ldr for large immediate)
wait_loop:
    subs r2, r2, #1
    bne wait_loop
    
    pop {pc}

test_fail:
    movs r0, #0xFF         @ Failure code
    b infinite_loop

@ Simple infinite loop for test completion  
infinite_loop:
    b infinite_loop

@ Exception handlers
.align 2
irq_handler:
    @ IRQ Handler - mark that we received interrupt
    movs r0, #0xAA         @ Marker value
    bx lr

systick_handler:
    @ SysTick Handler - mark that we received SysTick  
    movs r0, #0xBB         @ Marker value
    bx lr