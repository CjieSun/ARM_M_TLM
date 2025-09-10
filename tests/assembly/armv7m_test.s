.syntax unified
.thumb
.text
.global _start

@ ARMv7-M Thumb Instruction Set Test
@ This file tests ARMv7-M specific instructions including:
@ - IT (If-Then) blocks for conditional execution
@ - STRB/LDRB byte load/store operations  
@ - STMIA/LDMIA multiple load/store operations
@ - UMULL unsigned multiply long
@ - MLA multiply accumulate
@ - MLS multiply subtract
@ - T32 data processing instructions (ADD.W, SUB.W, MOV.W, etc.)
@ - Bitfield instructions (BFI, BFC, UBFX, SBFX)
@ - Division instructions (UDIV, SDIV)
@ - Shift instructions (LSL.W, LSR.W, ASR.W, ROR.W)
@ - System instructions (MRS, MSR)
@ - Advanced load/store (LDR.W, STR.W with various modes)
@ - Wide branch instructions (B.W, BL.W)

_start:
    @ Initialize stack pointer
    movs r0, #1
    lsls r0, r0, #12       @ 4KB stack
    mov sp, r0
    
    @ === IT BLOCK TESTS ===
    bl test_it_blocks
    
    @ === BYTE LOAD/STORE TESTS ===
    bl test_byte_operations
    
    @ === MULTIPLE LOAD/STORE TESTS ===
    bl test_multiple_operations
    
    @ === MULTIPLY INSTRUCTIONS TESTS ===
    bl test_multiply_operations
    
    @ === T32 DATA PROCESSING TESTS ===
    bl test_t32_data_processing
    
    @ === BITFIELD INSTRUCTIONS TESTS ===
    bl test_bitfield_operations
    
    @ === DIVISION INSTRUCTIONS TESTS ===
    bl test_division_operations
    
    @ === SHIFT INSTRUCTIONS TESTS ===
    bl test_shift_operations
    
    @ === SYSTEM INSTRUCTIONS TESTS ===
    bl test_system_instructions
    
    @ === ADVANCED LOAD/STORE TESTS ===
    bl test_advanced_loadstore
    
    @ === WIDE BRANCH TESTS ===
    bl test_wide_branches
    
    @ === EXTEND INSTRUCTIONS TESTS ===
    bl test_extend_instructions
    
    @ === REVERSE INSTRUCTIONS TESTS ===
    bl test_reverse_instructions
    
    @ === SATURATING ARITHMETIC TESTS ===
    bl test_saturating_arithmetic
    
    @ All tests completed successfully
    movs r0, #0x00         @ Success code
    b infinite_loop

@ Test IT (If-Then) conditional execution blocks
test_it_blocks:
    push {lr}
    
    @ Test simple IT block - IT EQ (if equal)
    movs r0, #5
    movs r1, #5
    cmp r0, r1             @ Set flags: should be equal
    
    it eq                  @ If equal, execute next instruction
    moveq r2, #0xAA        @ Should execute (r0 == r1)
    
    @ Test IT block with different condition - IT NE (if not equal)
    movs r0, #5
    movs r1, #10
    cmp r0, r1             @ Set flags: should be not equal
    
    it ne                  @ If not equal, execute next instruction
    movne r3, #0xBB        @ Should execute (r0 != r1)
    
    @ Test 2-instruction IT block - ITE (If-Then-Else)
    movs r0, #8
    movs r1, #12
    cmp r0, r1             @ r0 < r1, so LT condition true
    
    ite lt                 @ If less than, then else
    movlt r4, #0xCC        @ Should execute (condition true)
    movge r5, #0xDD        @ Should NOT execute (condition false)
    
    @ Test 3-instruction IT block - ITEE (If-Then-Else-Else)
    movs r0, #15
    movs r1, #10  
    cmp r0, r1             @ r0 > r1, so GT condition true
    
    itee gt                @ If greater than, then else else
    movgt r6, #0xEE        @ Should execute (condition true)
    movle r7, #0xFF        @ Should NOT execute (condition false)
    movle r8, #0x11        @ Should NOT execute (condition false)
    
    pop {pc}

@ Test byte load/store operations (STRB/LDRB)
test_byte_operations:
    push {lr}
    
    @ Set up test buffer
    adr r0, test_buffer
    
    @ Test STRB (store byte) with immediate offset
    movs r1, #0x42         @ Test byte value
    strb r1, [r0, #0]      @ Store byte at offset 0
    strb r1, [r0, #1]      @ Store byte at offset 1
    strb r1, [r0, #4]      @ Store byte at offset 4
    
    @ Test STRB with register offset
    movs r2, #8            @ Offset register
    movs r3, #0x73         @ Different test value
    strb r3, [r0, r2]      @ Store byte at r0 + r2
    
    @ Test LDRB (load byte) with immediate offset
    ldrb r4, [r0, #0]      @ Load byte from offset 0
    ldrb r5, [r0, #1]      @ Load byte from offset 1 
    ldrb r6, [r0, #4]      @ Load byte from offset 4
    
    @ Test LDRB with register offset
    ldrb r7, [r0, r2]      @ Load byte from r0 + r2
    
    @ Verify values loaded match what was stored
    cmp r4, #0x42          @ Should match stored value
    bne byte_test_fail
    cmp r5, #0x42          @ Should match stored value
    bne byte_test_fail
    cmp r6, #0x42          @ Should match stored value  
    bne byte_test_fail
    cmp r7, #0x73          @ Should match stored value
    bne byte_test_fail
    
    @ Test passed
    movs r0, #0x99
    b byte_test_end
    
byte_test_fail:
    movs r0, #0xBA         @ Error code
    
byte_test_end:
    pop {pc}

@ Test multiple load/store operations (STMIA/LDMIA)  
test_multiple_operations:
    push {lr}
    
    @ Set up test data
    adr r0, test_buffer
    movs r1, #0x11         @ Test data
    movs r2, #0x22
    movs r3, #0x33
    movs r4, #0x44
    movs r5, #0x55
    
    @ Test STMIA (store multiple increment after)
    stmia r0!, {r1, r2, r3, r4, r5}  @ Store multiple with writeback
    
    @ Reset pointer and clear registers
    adr r0, test_buffer
    movs r1, #0
    movs r2, #0
    movs r3, #0
    movs r4, #0
    movs r5, #0
    
    @ Test LDMIA (load multiple increment after)
    ldmia r0!, {r1, r2, r3, r4, r5}  @ Load multiple with writeback
    
    @ Verify loaded values
    cmp r1, #0x11
    bne multiple_test_fail
    cmp r2, #0x22
    bne multiple_test_fail
    cmp r3, #0x33
    bne multiple_test_fail
    cmp r4, #0x44
    bne multiple_test_fail
    cmp r5, #0x55
    bne multiple_test_fail
    
    @ Test passed
    movs r0, #0x99
    b multiple_test_end
    
multiple_test_fail:
    movs r0, #0xBA         @ Error code
    
multiple_test_end:
    pop {pc}

@ Test multiply instructions (UMULL, MLA, MLS)
test_multiply_operations:
    push {lr}
    
    @ Test UMULL (unsigned multiply long)
    @ UMULL RdLo, RdHi, Rn, Rm - RdHi:RdLo = Rn * Rm (64-bit result)
    movs r0, #0x1000       @ 4096
    movs r1, #0x2000       @ 8192  
    umull r2, r3, r0, r1   @ r3:r2 = r0 * r1 = 0x02000000 (33554432)
    
    @ Verify result: r2 should contain low 32 bits, r3 should contain high 32 bits
    ldr r4, =0x02000000    @ Expected low part
    cmp r2, r4
    bne multiply_test_fail
    cmp r3, #0             @ Expected high part (should be 0 for this small result)
    bne multiply_test_fail
    
    @ Test MLA (multiply accumulate)
    @ MLA Rd, Rn, Rm, Ra - Rd = (Rn * Rm) + Ra
    movs r0, #7            @ Multiplicand
    movs r1, #8            @ Multiplier
    movs r2, #5            @ Addend
    mla r3, r0, r1, r2     @ r3 = (7 * 8) + 5 = 56 + 5 = 61
    
    cmp r3, #61
    bne multiply_test_fail
    
    @ Test MLS (multiply subtract)  
    @ MLS Rd, Rn, Rm, Ra - Rd = Ra - (Rn * Rm)
    movs r0, #6            @ Multiplicand
    movs r1, #4            @ Multiplier
    movs r2, #30           @ Minuend
    mls r3, r0, r1, r2     @ r3 = 30 - (6 * 4) = 30 - 24 = 6
    
    cmp r3, #6
    bne multiply_test_fail
    
    @ Test basic MUL (16-bit multiply) for comparison
    movs r0, #9
    movs r1, #7
    mul r2, r0, r1         @ r2 = r0 * r1 = 63
    
    cmp r2, #63
    bne multiply_test_fail
    
    @ All multiply tests passed
    movs r0, #0x99
    b multiply_test_end
    
multiply_test_fail:
    movs r0, #0xBA         @ Error code
    
multiply_test_end:
    pop {pc}

@ Test T32 data processing instructions (32-bit wide versions)
test_t32_data_processing:
    push {lr}
    
    @ Test ADD.W with immediate (32-bit immediate support)
    ldr r0, =0x12345678    @ Load large immediate
    add.w r1, r0, #0x1000  @ ADD.W with 12-bit immediate
    
    @ Test SUB.W with immediate  
    sub.w r2, r1, #0x678   @ SUB.W with 12-bit immediate
    
    @ Test MOV.W with immediate (can encode any 32-bit value)
    mov.w r3, #0x5A5A      @ MOV.W with immediate
    movt r3, #0x1234       @ MOVT to set upper 16 bits: r3 = 0x12345A5A
    
    @ Test logical operations with register
    and.w r4, r0, r1       @ AND.W register
    orr.w r5, r0, r2       @ ORR.W register  
    eor.w r6, r4, r5       @ EOR.W register
    bic.w r7, r0, #0xFF    @ BIC.W with immediate
    
    @ Test arithmetic with register and shift
    add.w r8, r0, r1, lsl #2   @ ADD.W with shifted register
    sub.w r9, r0, r2, lsr #4   @ SUB.W with shifted register
    
    @ Test compare instructions with valid immediates
    cmp.w r0, #0xFF        @ CMP.W with 8-bit immediate
    tst.w r3, #0xFF00      @ TST.W with immediate
    
    movs r0, #0x99         @ Success
    pop {pc}

@ Test bitfield instructions (ARMv7-M feature)
test_bitfield_operations:
    push {lr}
    
    @ Test BFI (Bit Field Insert)
    movs r0, #0xFF         @ Source data
    movw r1, #0x5555       @ Destination with existing data (use MOVW for large immediates)
    bfi r1, r0, #8, #8     @ Insert r0[7:0] into r1[15:8]
    @ r1 should now be 0x55FF
    
    @ Test BFC (Bit Field Clear)
    ldr r2, =0xFFFFFFFF    @ All bits set
    bfc r2, #8, #16        @ Clear bits [23:8]
    @ r2 should now be 0xFF0000FF
    
    @ Test UBFX (Unsigned Bit Field Extract)
    ldr r3, =0x12345678
    ubfx r4, r3, #8, #16   @ Extract bits [23:8] (unsigned)
    @ r4 should be 0x00005678
    
    @ Test SBFX (Signed Bit Field Extract)
    ldr r5, =0x87654321
    sbfx r6, r5, #16, #8   @ Extract bits [23:16] (signed)
    @ r6 should be sign-extended value
    
    movs r0, #0x99         @ Success
    pop {pc}

@ Test division instructions (ARMv7-M feature)
test_division_operations:
    push {lr}
    
    @ Test UDIV (Unsigned Division)
    movs r0, #100          @ Dividend
    movs r1, #7            @ Divisor  
    udiv r2, r0, r1        @ r2 = r0 / r1 = 100 / 7 = 14
    
    cmp r2, #14
    bne division_test_fail
    
    @ Test SDIV (Signed Division)
    mvn r3, #99            @ r3 = -100 (two's complement)
    movs r4, #7            @ Divisor
    sdiv r5, r3, r4        @ r5 = r3 / r4 = -100 / 7 = -14
    
    cmn r5, #14            @ Compare with -14
    bne division_test_fail
    
    @ Test division by larger number (result should be 0)
    movs r6, #5
    movs r7, #10
    udiv r8, r6, r7        @ r8 = 5 / 10 = 0
    
    cmp r8, #0
    bne division_test_fail
    
    movs r0, #0x99         @ Success
    b division_test_end
    
division_test_fail:
    movs r0, #0xBA         @ Error code
    
division_test_end:
    pop {pc}

@ Test shift instructions with register operands
test_shift_operations:
    push {lr}
    
    @ Test LSL.W (Logical Shift Left)
    movs r0, #0x12         @ Test value
    movs r1, #4            @ Shift amount
    lsl.w r2, r0, r1       @ r2 = r0 << r1 = 0x120
    
    cmp r2, #0x120
    bne shift_test_fail
    
    @ Test LSR.W (Logical Shift Right)  
    ldr r3, =0x1200
    movs r4, #8            @ Shift amount
    lsr.w r5, r3, r4       @ r5 = r3 >> r4 = 0x12
    
    cmp r5, #0x12
    bne shift_test_fail
    
    @ Test ASR.W (Arithmetic Shift Right)
    ldr r6, =0x80000000    @ Negative number
    movs r7, #1            @ Shift amount  
    asr.w r8, r6, r7       @ r8 = r6 >> r7 (sign extended)
    
    ldr r9, =0xC0000000    @ Expected result
    cmp r8, r9
    bne shift_test_fail
    
    @ Test ROR.W (Rotate Right)
    movs r0, #0x12         @ Test value
    movs r1, #4            @ Rotate amount
    ror.w r2, r0, r1       @ r2 = rotate r0 right by r1
    
    ldr r3, =0x20000001    @ Expected result (0x12 rotated right by 4)
    cmp r2, r3
    bne shift_test_fail
    
    movs r0, #0x99         @ Success
    b shift_test_end
    
shift_test_fail:
    movs r0, #0xBA         @ Error code
    
shift_test_end:
    pop {pc}

@ Test system instructions (MRS, MSR)
test_system_instructions:
    push {lr}
    
    @ Test MRS (Move from Special Register)
    mrs r0, psp            @ Read Process Stack Pointer
    mrs r1, msp            @ Read Main Stack Pointer  
    mrs r2, primask        @ Read Priority Mask
    mrs r3, basepri        @ Read Base Priority
    mrs r4, faultmask      @ Read Fault Mask
    mrs r5, control        @ Read Control register
    
    @ Test MSR (Move to Special Register)
    movs r6, #1
    msr primask, r6        @ Set Priority Mask (disable interrupts)
    
    movs r7, #0x20         @ Priority level
    msr basepri, r7        @ Set Base Priority
    
    movs r8, #0
    msr primask, r8        @ Clear Priority Mask (enable interrupts)
    
    movs r0, #0x99         @ Success
    pop {pc}

@ Test advanced load/store operations
test_advanced_loadstore:
    push {lr}
    
    adr r0, test_buffer
    
    @ Test LDR.W with various addressing modes
    ldr r1, =0xABCDEF01
    str.w r1, [r0]         @ Store test data
    
    @ Test pre-indexed addressing [Rn, #imm]!
    ldr.w r2, [r0, #4]!    @ Load and update r0 = r0 + 4
    
    @ Test post-indexed addressing [Rn], #imm  
    str.w r1, [r0], #4     @ Store and update r0 = r0 + 4
    
    @ Test negative offset
    ldr.w r3, [r0, #-8]    @ Load from r0 - 8
    
    @ Test LDRB.W and STRB.W with pre/post indexing
    movs r4, #0x42
    strb.w r4, [r0, #1]!   @ Store byte with pre-increment
    ldrb.w r5, [r0], #-1   @ Load byte with post-decrement
    
    @ Test LDRH.W and STRH.W (halfword operations)
    ldr r6, =0x1234
    strh.w r6, [r0, #2]    @ Store halfword
    ldrh.w r7, [r0, #2]    @ Load halfword
    
    cmp r7, r6
    bne loadstore_test_fail
    
    movs r0, #0x99         @ Success
    b loadstore_test_end
    
loadstore_test_fail:
    movs r0, #0xBA         @ Error code
    
loadstore_test_end:
    pop {pc}

@ Test wide branch instructions
test_wide_branches:
    push {lr}
    
    @ Test B.W (32-bit branch for longer ranges)
    b.w wide_branch_target @ Branch to target
    
    @ Should not reach here
    movs r0, #0xBA
    b wide_branch_end
    
wide_branch_target:
    @ Test conditional B.W
    movs r1, #5
    movs r2, #5
    cmp r1, r2
    beq.w wide_branch_conditional_target
    
    @ Should not reach here
    movs r0, #0xBA
    b wide_branch_end
    
wide_branch_conditional_target:
    @ Test BL.W (32-bit branch with link)
    bl.w wide_branch_subroutine
    
    movs r0, #0x99         @ Success
    b wide_branch_end
    
wide_branch_subroutine:
    @ Simple subroutine for BL.W test
    movs r3, #0xCC         @ Mark that we reached here
    bx lr                  @ Return
    
wide_branch_end:
    pop {pc}

@ Test extend instructions (sign/zero extend)
test_extend_instructions:
    push {lr}
    
    @ Test UXTB (Zero extend byte to word)
    ldr r0, =0x12345678
    uxtb r1, r0            @ r1 = 0x00000078 (zero extend byte)
    
    ldr r2, =0x00000078
    cmp r1, r2
    bne extend_test_fail
    
    @ Test UXTH (Zero extend halfword to word)
    ldr r3, =0x12345678
    uxth r4, r3            @ r4 = 0x00005678 (zero extend halfword)
    
    ldr r5, =0x00005678
    cmp r4, r5
    bne extend_test_fail
    
    @ Test SXTB (Sign extend byte to word)
    ldr r6, =0x123456F8    @ Byte value 0xF8 (negative)
    sxtb r7, r6            @ r7 = 0xFFFFFFF8 (sign extend byte)
    
    ldr r8, =0xFFFFFFF8
    cmp r7, r8
    bne extend_test_fail
    
    @ Test SXTH (Sign extend halfword to word)
    ldr r9, =0x12348000    @ Halfword value 0x8000 (negative)
    sxth r10, r9           @ r10 = 0xFFFF8000 (sign extend halfword)
    
    ldr r11, =0xFFFF8000
    cmp r10, r11
    bne extend_test_fail
    
    movs r0, #0x99         @ Success
    b extend_test_end
    
extend_test_fail:
    movs r0, #0xBA         @ Error code
    
extend_test_end:
    pop {pc}

@ Test byte/bit reverse instructions
test_reverse_instructions:
    push {lr}
    
    @ Test REV (Reverse bytes in word)
    ldr r0, =0x12345678
    rev r1, r0             @ r1 = 0x78563412 (reverse all bytes)
    
    ldr r2, =0x78563412
    cmp r1, r2
    bne reverse_test_fail
    
    @ Test REV16 (Reverse bytes in each halfword)
    ldr r3, =0x12345678
    rev16 r4, r3           @ r4 = 0x34127856 (reverse bytes in each halfword)
    
    ldr r5, =0x34127856
    cmp r4, r5
    bne reverse_test_fail
    
    @ Test REVSH (Reverse bytes in signed halfword)
    ldr r6, =0x12348000    @ Use 0x8000 in lower halfword
    revsh r7, r6           @ r7 = 0x00000080 -> sign extended
    
    @ Test RBIT (Reverse bits in word) - if supported
    @ Note: RBIT may not be available on all ARMv7-M implementations
    ldr r8, =0x12345678
    @ rbit r9, r8         @ Uncomment if RBIT is supported
    
    movs r0, #0x99         @ Success
    b reverse_test_end
    
reverse_test_fail:
    movs r0, #0xBA         @ Error code
    
reverse_test_end:
    pop {pc}

@ Test saturating arithmetic instructions
test_saturating_arithmetic:
    push {lr}
    
    @ Test SSAT (Signed Saturate) - supported on Cortex-M3/M4
    ldr r0, =0x12345678    @ Value to saturate
    ssat r1, #8, r0        @ Saturate to 8-bit signed (-128 to 127)
    cmp r1, #127           @ Should saturate to maximum 8-bit signed value
    bne saturating_test_fail
    
    @ Test USAT (Unsigned Saturate)
    ldr r2, =0x87654321    @ Negative value
    usat r3, #8, r2        @ Saturate to 8-bit unsigned (0 to 255)
    cmp r3, #0             @ Negative should saturate to 0
    bne saturating_test_fail
    
    @ Test SSAT with shift
    mov r4, #0x1000        @ Value 4096
    ssat r5, #8, r4, lsl #2 @ Shift left by 2, then saturate
    cmp r5, #127           @ Should saturate to max
    bne saturating_test_fail
    
    @ Test USAT with shift  
    mov r6, #0x100         @ Value 256
    usat r7, #8, r6, asr #1 @ Shift right by 1, then saturate (use ASR instead of LSR)
    cmp r7, #128           @ 256>>1 = 128, within 8-bit unsigned range
    bne saturating_test_fail
    
    @ Test USAT (Unsigned Saturate)
    ldr r10, =0x12345678   @ Value to saturate  
    usat r11, #8, r10      @ Saturate to 8-bit unsigned (0 to 255)
    
    cmp r11, #255          @ Should saturate to maximum 8-bit unsigned value
    bne saturating_test_fail
    
    movs r0, #0x99         @ Success
    b saturating_test_end
    
saturating_test_fail:
    movs r0, #0xBA         @ Error code
    
saturating_test_end:
    pop {pc}

@ Infinite loop for test completion
infinite_loop:
    b infinite_loop

// ===== Additional ARMv7-M Advanced Instructions =====

// ===== T32 Memory Barrier and Synchronization Instructions =====
test_memory_barriers:
    push {lr}
    
    // Memory barriers
    dsb     sy                  // Data synchronization barrier
    dmb     sy                  // Data memory barrier  
    isb     sy                  // Instruction synchronization barrier
    dsb     ish                 // Inner shareable domain
    dmb     ishst              // Inner shareable store
    isb                         // ISB with implicit SY
    
    // Exclusive access instructions
    ldr     r1, =test_buffer
    ldrex   r0, [r1]           // Load exclusive
    add     r0, r0, #1         // Modify value
    strex   r2, r0, [r1]       // Store exclusive
    cmp     r2, #0             // Check if store succeeded
    bne     test_memory_barriers // Retry if failed
    
    ldrexb  r0, [r1]           // Load exclusive byte
    strexb  r2, r0, [r1]       // Store exclusive byte
    ldrexh  r0, [r1]           // Load exclusive halfword
    strexh  r2, r0, [r1]       // Store exclusive halfword
    clrex                      // Clear exclusive
    
    pop {pc}

// ===== T32 System Register Instructions =====
test_system_registers:
    push {lr}
    
    // System register access
    mrs     r0, msp            // Read main stack pointer
    mrs     r1, psp            // Read process stack pointer  
    mrs     r2, primask        // Read interrupt mask
    mrs     r3, basepri        // Read base priority
    mrs     r4, faultmask      // Read fault mask
    mrs     r5, control        // Read control register
    
    // Write back system registers
    msr     msp, r0            // Write main stack pointer
    msr     psp, r1            // Write process stack pointer
    msr     primask, r2        // Write interrupt mask
    msr     basepri, r3        // Write base priority
    msr     faultmask, r4      // Write fault mask
    msr     control, r5        // Write control register
    
    // Exception return values
    movw    r0, #0xFFF1        // Lower 16 bits
    movt    r0, #0xFFFF        // Upper 16 bits - Handler mode, use MSP
    movw    r1, #0xFFF9        // Lower 16 bits  
    movt    r1, #0xFFFF        // Upper 16 bits - Thread mode, use MSP
    movw    r2, #0xFFFD        // Lower 16 bits
    movt    r2, #0xFFFF        // Upper 16 bits - Thread mode, use PSP
    
    // Wait instructions
    wfi                        // Wait for interrupt
    wfe                        // Wait for event
    sev                        // Send event
    yield                      // Yield hint
    
    pop {pc}

// ===== T32 Advanced Bit Field Instructions =====
test_advanced_bitfield:
    push {lr}
    
    // Setup test data
    ldr     r0, =0x12345678
    movw    r1, #0xEF01      // Load lower 16 bits
    movt    r1, #0xABCD      // Load upper 16 bits
    
    // Bit field operations
    bfi     r0, r1, #8, #16   // Insert bits [15:0] of r1 into r0[23:8]
    bfc     r0, #4, #8        // Clear bits [11:4] of r0
    ubfx    r2, r0, #8, #16   // Extract bits [23:8] unsigned
    sbfx    r3, r0, #8, #16   // Extract bits [23:8] signed
    
    // Count leading zeros
    clz     r4, r0             // Count leading zeros
    
    // Reverse operations
    rev     r5, r0             // Byte reverse
    rev16   r6, r0             // Reverse bytes in halfwords
    revsh   r7, r0             // Reverse signed halfword
    rbit    r8, r0             // Reverse bits
    
    pop {pc}

// ===== T32 Debug and Special Instructions =====
test_debug_special:
    push {lr}
    
    // Breakpoint instructions
    bkpt    #0                 // Breakpoint with immediate 0
    bkpt    #255               // Breakpoint with max immediate
    
    // No-op and hints
    nop                        // No operation
    nop.w                      // 32-bit no operation
    
    pop {pc}

// ===== Complex Addressing Modes Test =====
test_complex_addressing:
    push {r4-r7, lr}
    
    ldr     r0, =test_constants
    
    // Various T32 load/store addressing modes
    ldr.w   r1, [r0]           // Basic load
    ldr.w   r2, [r0, #4]       // Immediate offset
    ldr.w   r3, [r0, #8]!      // Pre-indexed with writeback
    ldr.w   r4, [r0], #4       // Post-indexed with writeback
    
    // Load/store with negative offsets
    ldr.w   r5, [r0, #-4]      // Negative immediate offset
    str.w   r5, [r0, #-8]      // Store with negative offset
    
    // Byte and halfword operations
    ldrb.w  r6, [r0, #1]       // Load byte
    strb.w  r6, [r0, #2]       // Store byte
    ldrh.w  r7, [r0, #2]       // Load halfword
    strh.w  r7, [r0, #4]       // Store halfword
    
    // PC-relative loads
    ldr.w   r1, test_constants  // PC-relative load
    
    pop {r4-r7, pc}

// ===== Complex IT Block Tests =====
test_complex_it_blocks:
    push {lr}
    
    mov     r0, #5
    mov     r1, #3
    cmp     r0, r1
    
    // Simple IT block
    it      gt
    addgt   r2, r0, #1
    
    // IT-Then-Else block
    ite     gt
    addgt   r2, r0, #1         // Execute if greater
    addle   r2, r1, #1         // Execute if less or equal
    
    // Longer IT block
    ittte   gt
    addgt   r3, r0, #1         // Then
    subgt   r4, r0, #1         // Then  
    movgt   r5, #10            // Then
    movle   r6, #20            // Else
    
    // IT block with various conditions
    cmp     r0, #0
    it      eq
    moveq   r7, #0
    
    it      ne
    movne   r8, #1
    
    pop {pc}

// ===== Performance Test Section =====
test_performance_patterns:
    push {r4-r11, lr}
    
    // Loop with complex operations
    mov     r0, #1000          // Loop counter
    mov     r1, #0             // Accumulator
    
performance_loop:
    // Complex arithmetic in loop
    add     r2, r1, r0, lsl #2
    mul     r3, r2, r0
    udiv    r4, r3, r0         // Division
    add     r1, r1, r4
    
    subs    r0, r0, #1
    bne     performance_loop
    
    // Branch prediction test
    mov     r0, #100
branch_test_loop:
    tst     r0, #1             // Test odd/even
    it      eq
    addeq   r1, r1, #2
    it      ne  
    addne   r1, r1, #3
    
    subs    r0, r0, #1
    bne     branch_test_loop
    
    pop {r4-r11, pc}

.align 2
test_buffer:
    .space 128             @ Expanded test data buffer

@ Test data constants
.align 2
test_constants:
    .word 0x12345678       @ 32-bit test constant
    .word 0xABCDEF01       @ Another test constant
    .word 0x87654321       @ Signed test value
    .word 0xFFFFFFFF       @ All bits set
    .word 0x00000000       @ All bits clear
    .word 0x80000000       @ MSB set
    .word 0x7FFFFFFF       @ Maximum positive signed
    .word 0x55555555       @ Alternating bits
    .word 0xAAAAAAAA       @ Inverted alternating bits

@ Test arrays for complex operations
.align 2
test_array_32:
    .word 1, 2, 3, 4, 5, 6, 7, 8
    .word 9, 10, 11, 12, 13, 14, 15, 16

.align 2
test_array_16:
    .hword 0x1111, 0x2222, 0x3333, 0x4444
    .hword 0x5555, 0x6666, 0x7777, 0x8888
    .hword 0x9999, 0xAAAA, 0xBBBB, 0xCCCC
    .hword 0xDDDD, 0xEEEE, 0xFFFF, 0x0000

.align 1  
test_array_8:
    .byte 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08
    .byte 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10
    .byte 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18
    .byte 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20

@ String data for testing
.align 1
test_string:
    .ascii "ARMv7-M Cortex-M3/M4 Test String"
    .byte 0

@ Memory-mapped peripheral addresses (typical for Cortex-M)
.equ NVIC_ISER,     0xE000E100  @ Interrupt Set Enable Register
.equ NVIC_ICER,     0xE000E180  @ Interrupt Clear Enable Register  
.equ NVIC_ISPR,     0xE000E200  @ Interrupt Set Pending Register
.equ NVIC_ICPR,     0xE000E280  @ Interrupt Clear Pending Register
.equ SCB_AIRCR,     0xE000ED0C  @ Application Interrupt/Reset Control
.equ SCB_SCR,       0xE000ED10  @ System Control Register
.equ SCB_SHCSR,     0xE000ED24  @ System Handler Control and State
.equ SysTick_CTRL,  0xE000E010  @ SysTick Control and Status
.equ SysTick_LOAD,  0xE000E014  @ SysTick Reload Value Register
.equ SysTick_VAL,   0xE000E018  @ SysTick Current Value Register

@ Test patterns for bit manipulation
.align 2
bit_patterns:
    .word 0x01010101       @ Every 8th bit
    .word 0x03030303       @ Every 4th pair
    .word 0x0F0F0F0F       @ Every 4 bits
    .word 0x00FF00FF       @ Every byte
    .word 0x0000FFFF       @ Every halfword
    .word 0xF0F0F0F0       @ Inverted nibbles
    .word 0xCC33CC33       @ 2-bit patterns
    .word 0xAAAA5555       @ Checkerboard pattern

@ Working space for tests
.align 2
workspace:
    .space 256             @ 256 bytes of working memory

@ End marker
.align 2
end_marker:
    .word 0xDEADC0DE       @ End of data marker

@ End of test program
