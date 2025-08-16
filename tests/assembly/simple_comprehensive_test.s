.syntax unified
.thumb
.text
.global _start

@ Simple comprehensive test combining basic operations
_start:
    @ Initialize stack pointer
    movs r0, #1
    lsls r0, r0, #12       @ 4KB stack
    mov sp, r0
    
    @ === BASIC DATA PROCESSING ===
    bl test_data_processing
    
    @ === BASIC LOAD/STORE ===  
    bl test_load_store
    
    @ === BASIC BRANCHES ===
    bl test_branches
    
    @ All tests completed successfully
    movs r0, #0x00         @ Success code
    b infinite_loop

@ Data Processing Test Subroutine
test_data_processing:
    push {lr}
    
    @ Basic arithmetic
    movs r0, #10
    movs r1, #5
    adds r2, r0, r1        @ r2 = 15
    subs r3, r0, r1        @ r3 = 5
    
    @ Bitwise operations  
    movs r0, #0x5A
    movs r1, #0x3C
    ands r2, r0            @ AND operation
    orrs r3, r0            @ OR operation
    
    pop {pc}

@ Load/Store Test Subroutine
test_load_store:
    push {lr}
    
    @ Set up test data
    adr r0, test_data
    movs r1, #0x12
    movs r2, #0x34
    
    @ Store operations
    str r1, [r0, #0]       @ Store word
    strb r2, [r0, #4]      @ Store byte
    
    @ Load operations
    ldr r3, [r0, #0]       @ Load word
    ldrb r4, [r0, #4]      @ Load byte
    
    pop {pc}

@ Branch Test Subroutine
test_branches:
    push {lr}
    
    @ Test conditional branch
    movs r0, #10
    movs r1, #5
    cmp r0, r1             @ Compare
    bne branch_taken       @ Should branch (not equal)
    
    @ Should not reach here
    movs r0, #0xBA
    b branch_end
    
branch_taken:
    movs r0, #0x99         @ Success marker
    
branch_end:
    pop {pc}

infinite_loop:
    b infinite_loop

.align 2
test_data:
    .space 64              @ Test data buffer
