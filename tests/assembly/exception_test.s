.syntax unified
.thumb
.text
.global _start

@ Exception handling test
_start:
    @ Initialize stack pointer to top of RAM
    ldr r0, =0x20010000
    mov sp, r0
    
    @ Set up exception handlers in vector table
    @ Note: In a real system, the vector table would be set up properly
    @ For this test, we'll trigger exceptions and verify they work
    
    @ Test SVC exception
    bl test_svc_exception
    
    @ Test basic exception handling
    movs r0, #0x42         @ Set test value
    svc #0x10              @ Trigger SVC exception
    
    @ If we reach here, SVC handler returned properly
    movs r0, #0x00         @ Success code
    b infinite_loop

@ Test SVC exception handling
test_svc_exception:
    push {lr}
    
    @ Prepare test data
    movs r0, #0x11
    movs r1, #0x22
    movs r2, #0x33
    
    @ This will be processed by the CPU's execute_exception method
    svc #0x01
    
    @ Verify we returned from exception
    movs r0, #0x55         @ Marker value
    
    pop {pc}

@ Simple infinite loop for test completion
infinite_loop:
    b infinite_loop

@ Exception handlers (would normally be in vector table)
.align 2
nmi_handler:
    @ NMI Handler - just return for test
    bx lr

hardfault_handler:  
    @ HardFault Handler - just return for test
    bx lr

svc_handler:
    @ SVC Handler - just return for test  
    bx lr

pendsv_handler:
    @ PendSV Handler - just return for test
    bx lr

systick_handler:
    @ SysTick Handler - just return for test
    bx lr