/* Copyright Statement:
 *
 * This software/firmware and related documentation ("AutoChips Software") are
 * protected under relevant copyright laws. The information contained herein is
 * confidential and proprietary to AutoChips Inc. and/or its licensors. Without
 * the prior written permission of AutoChips inc. and/or its licensors, any
 * reproduction, modification, use or disclosure of AutoChips Software, and
 * information contained herein, in whole or in part, shall be strictly
 * prohibited.
 *
 * AutoChips Inc. (C) 2023. All rights reserved.
 *
 * BY OPENING THIS FILE, RECEIVER HEREBY UNEQUIVOCALLY ACKNOWLEDGES AND AGREES
 * THAT THE SOFTWARE/FIRMWARE AND ITS DOCUMENTATIONS ("AUTOCHIPS SOFTWARE")
 * RECEIVED FROM AUTOCHIPS AND/OR ITS REPRESENTATIVES ARE PROVIDED TO RECEIVER
 * ON AN "AS-IS" BASIS ONLY. AUTOCHIPS EXPRESSLY DISCLAIMS ANY AND ALL
 * WARRANTIES, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE OR
 * NONINFRINGEMENT. NEITHER DOES AUTOCHIPS PROVIDE ANY WARRANTY WHATSOEVER WITH
 * RESPECT TO THE SOFTWARE OF ANY THIRD PARTY WHICH MAY BE USED BY,
 * INCORPORATED IN, OR SUPPLIED WITH THE AUTOCHIPS SOFTWARE, AND RECEIVER AGREES
 * TO LOOK ONLY TO SUCH THIRD PARTY FOR ANY WARRANTY CLAIM RELATING THERETO.
 * RECEIVER EXPRESSLY ACKNOWLEDGES THAT IT IS RECEIVER'S SOLE RESPONSIBILITY TO
 * OBTAIN FROM ANY THIRD PARTY ALL PROPER LICENSES CONTAINED IN AUTOCHIPS
 * SOFTWARE. AUTOCHIPS SHALL ALSO NOT BE RESPONSIBLE FOR ANY AUTOCHIPS SOFTWARE
 * RELEASES MADE TO RECEIVER'S SPECIFICATION OR TO CONFORM TO A PARTICULAR
 * STANDARD OR OPEN FORUM. RECEIVER'S SOLE AND EXCLUSIVE REMEDY AND AUTOCHIPS'S
 * ENTIRE AND CUMULATIVE LIABILITY WITH RESPECT TO THE AUTOCHIPS SOFTWARE
 * RELEASED HEREUNDER WILL BE, AT AUTOCHIPS'S OPTION, TO REVISE OR REPLACE THE
 * AUTOCHIPS SOFTWARE AT ISSUE, OR REFUND ANY SOFTWARE LICENSE FEES OR SERVICE
 * CHARGE PAID BY RECEIVER TO AUTOCHIPS FOR SUCH AUTOCHIPS SOFTWARE AT ISSUE.
 */

// Amount of memory (in bytes) allocated for Stack
// Tailor this value to your application needs
// <h> Stack Configuration
//   <o> Stack Size (in Bytes) <0x0-0xFFFFFFFF:8>
// </h>

    .file "startup_ac7840x.s"
    .syntax unified

    .cpu cortex-m0plus
    .text

    .thumb

    .section .isr_vector
    .align 2
    .globl __Vectors
__Vectors:
    .long   __StackTop                                      // Top of Stack
    .long   Reset_Handler                                   // Reset Handler
    .long   NMI_Handler                                     // NMI Handler
    .long   HardFault_Handler                               // Hard Fault Handler
    .long   MemManage_Handler                               // MPU Fault Handler
    .long   BusFault_Handler                                // Bus Fault Handler
    .long   UsageFault_Handler                              // Usage Fault Handler
    .long   0                                               // Reserved
    .long   0                                               // Reserved
    .long   0                                               // Reserved
    .long   0                                               // Reserved
    .long   SVC_Handler                                     // SVCall Handler
    .long   DebugMon_Handler                                // Debug Monitor Handler
    .long   0                                               // Reserved
    .long   PendSV_Handler                                  // PendSV Handler
    .long   SysTick_Handler                                 // SysTick Handler

// External interrupts
    .long     DMA0_Channel0_IRQHandler            //DMA channel 0 Interrupt
    .long     DMA0_Channel1_IRQHandler            //DMA channel 1 Interrupt
    .long     DMA0_Channel2_IRQHandler            //DMA channel 2 Interrupt
    .long     DMA0_Channel3_IRQHandler            //DMA channel 3 Interrupt
    .long     DMA0_Channel4_IRQHandler            //DMA channel 4 Interrupt
    .long     DMA0_Channel5_IRQHandler            //DMA channel 5 Interrupt
    .long     DMA0_Channel6_IRQHandler            //DMA channel 6 Interrupt
    .long     DMA0_Channel7_IRQHandler            //DMA channel 7 Interrupt
    .long     DMA0_Channel8_IRQHandler            //DMA channel 8 Interrupt
    .long     DMA0_Channel9_IRQHandler            //DMA channel 9 Interrupt
    .long     DMA0_Channel10_IRQHandler           //DMA channel 10 Interrupt
    .long     DMA0_Channel11_IRQHandler           //DMA channel 11 Interrupt
    .long     DMA0_Channel12_IRQHandler           //DMA channel 12 Interrupt
    .long     DMA0_Channel13_IRQHandler           //DMA channel 13 Interrupt
    .long     DMA0_Channel14_IRQHandler           //DMA channel 14 Interrupt
    .long     DMA0_Channel15_IRQHandler           //DMA channel 15 Interrupt
    .long     PORTA_IRQHandler                    //Port A pin detect interruptInterrupt
    .long     PORTB_IRQHandler                    //Port B pin detect interrupt
    .long     PORTC_IRQHandler                    //Port C pin detect interrupt
    .long     PORTD_IRQHandler                    //Port D pin detect interrupt
    .long     PORTE_IRQHandler                    //Port E pin detect interrupt
    .long     UART0_IRQHandler                    //UART0 Transmit / Receive Interrupt
    .long     UART1_IRQHandler                    //UART1 Transmit / Receive Interrupt
    .long     UART2_IRQHandler                    //UART2 Transmit / Receive Interrupt
    .long     UART3_IRQHandler                    //UART3 Transmit / Receive Interrupt
    .long     0                                   //Reserved
    .long     0                                   //Reserved
    .long     SPI0_IRQHandler                     //SPI0 Interrupt
    .long     SPI1_IRQHandler                     //SPI1 Interrupt
    .long     SPI2_IRQHandler                     //SPI2 Interrupt
    .long     0                                   //Reserved
    .long     I2C0_IRQHandler                     //I2C0 Interrupt
    .long     0                                   //Reserved
    .long     EIO_IRQHandler                      //EIO Interrupt
    .long     CAN0_IRQHandler                     //CAN0 Interrupt
    .long     CAN0_Wakeup_IRQHandler              //CAN0 Wakeup Interrupt
    .long     CAN1_IRQHandler                     //CAN1 Interrupt
    .long     CAN1_Wakeup_IRQHandler              //CAN1 Wakeup Interrupt
    .long     CAN2_IRQHandler                     //CAN2 Interrupt
    .long     CAN2_Wakeup_IRQHandler              //CAN2 Wakeup Interrupt
    .long     CAN3_IRQHandler                     //CAN3 Interrupt
    .long     CAN3_Wakeup_IRQHandler              //CAN3 Wakeup Interrupt
    .long     0                                   //Reserved
    .long     0                                   //Reserved
    .long     0                                   //Reserved
    .long     0                                   //Reserved
    .long     PDT0_IRQHandler                     //PDT0 Interrupt
    .long     PDT1_IRQHandler                     //PDT1 Interrupt
    .long     ADC0_IRQHandler                     //ADC0 Interrupt
    .long     ADC1_IRQHandler                     //ADC1 Interrupt
    .long     ACMP0_IRQHandler                    //ACMP0 Interrupt
    .long     WDG_IRQHandler                      //WDG Interrupt
    .long     EWDG_IRQHandler                     //EWDG Interrupt
    .long     MCM_IRQHandler                      //MCM Interrupt
    .long     LVD_IRQHandler                      //LVD Interrupt
    .long     SPM_IRQHandler                      //SPM Interrupt
    .long     RCM_IRQHandler                      //RCM Interrupt
    .long     PWM0_Overflow_IRQHandler            //PWM0 Overflow Interrupt
    .long     PWM0_Channel_IRQHandler             //PWM0 Channel Interrupt
    .long     PWM0_Fault_IRQHandler               //PWM0 Fault Interrupt
    .long     PWM1_Overflow_IRQHandler            //PWM1 Overflow Interrupt
    .long     PWM1_Channel_IRQHandler             //PWM1 Channel Interrupt
    .long     PWM1_Fault_IRQHandler               //PWM1 Fault Interrupt
    .long     PWM2_Overflow_IRQHandler            //PWM2 Overflow Interrupt
    .long     PWM2_Channel_IRQHandler             //PWM2 Channel Interrupt
    .long     PWM2_Fault_IRQHandler               //PWM2 Fault Interrupt
    .long     PWM3_Overflow_IRQHandler            //PWM3 Overflow Interrupt
    .long     PWM3_Channel_IRQHandler             //PWM3 Channel Interrupt
    .long     PWM3_Fault_IRQHandler               //PWM3 Fault Interrupt
    .long     PWM4_Overflow_IRQHandler            //PWM4 Overflow Interrupt
    .long     PWM4_Channel_IRQHandler             //PWM4 Channel Interrupt
    .long     PWM4_Fault_IRQHandler               //PWM4 Fault Interrupt
    .long     PWM5_Overflow_IRQHandler            //PWM5 Overflow Interrupt
    .long     PWM5_Channel_IRQHandler             //PWM5 Channel Interrupt
    .long     PWM5_Fault_IRQHandler               //PWM5 Fault Interrupt
    .long     0                                   //Reserved
    .long     0                                   //Reserved
    .long     0                                   //Reserved
    .long     0                                   //Reserved
    .long     0                                   //Reserved
    .long     0                                   //Reserved
    .long     RTC_IRQHandler                      //RTC Interrupt
    .long     PCT_IRQHandler                      //PCT Interrupt
    .long     TIMER_Channel0_IRQHandler           //TIMER Channel0 Interrupt
    .long     TIMER_Channel1_IRQHandler           //TIMER Channel1 Interrupt
    .long     TIMER_Channel2_IRQHandler           //TIMER Channel2 Interrupt
    .long     TIMER_Channel3_IRQHandler           //TIMER Channel3 Interrupt
    .long     CSE_IRQHandler                      //CSE Interrupt
    .long     FLASH_ECC_IRQHandler                //Flash ECC Interrupt
    .long     FLASH_IRQHandler                    //Flash Interrupt
    .long     FLASH_Collision_IRQHandler          //Flash Collision Interrupt
    .long     ECC_1BIT_IRQHandler                 //ECC 1BIT Interrupt
    .long     ECC_2BIT_IRQHandler                 //ECC 2BIT Interrupt

    .long     DefaultISR
    .size    __Vectors, . - __Vectors

    .text
    .thumb

// Reset Handler
    .thumb_func
    .align 2
    .globl   Reset_Handler
    .weak    Reset_Handler
    .type    Reset_Handler, %function
    
@ Exception handling test
Reset_Handler:
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
    @ Wait counter: 1000 = 250 << 2; avoid Thumb-2 MOVW on v6-M
    movs r2, #250
    lsls r2, r2, #2
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

    .align  1
    .thumb
    .thumb_func
    .weak DefaultISR
    .type DefaultISR, %function
DefaultISR:
    b       .
    .size DefaultISR, . - DefaultISR

/*    Macro to define default handlers. Default handler
 *    will be weak symbol and just dead loops. They can be
 *    overwritten by other handlers */
    .macro def_irq_handler    handler_name
    .weak \handler_name
    .set  \handler_name, DefaultISR
    .endm

// Exception Handlers
    def_irq_handler    NMI_Handler                         //NMI Handler
    def_irq_handler    HardFault_Handler                   //Hard Fault Handler
    def_irq_handler    MemManage_Handler                   //MPU Fault Handler
    def_irq_handler    BusFault_Handler                    //Bus Fault Handler
    def_irq_handler    UsageFault_Handler                  //Usage Fault Handler
    def_irq_handler    SVC_Handler                         //SVCall Handler
    def_irq_handler    DebugMon_Handler                    //Debug Monitor Handle
    def_irq_handler    PendSV_Handler                      //PendSV Handler
    //def_irq_handler    SysTick_Handler                     //SysTick Handler
    //def_irq_handler    DMA0_Channel0_IRQHandler            //DMA channel 0 Interrupt
    def_irq_handler    DMA0_Channel1_IRQHandler            //DMA channel 1 Interrupt
    def_irq_handler    DMA0_Channel2_IRQHandler            //DMA channel 2 Interrupt
    def_irq_handler    DMA0_Channel3_IRQHandler            //DMA channel 3 Interrupt
    def_irq_handler    DMA0_Channel4_IRQHandler            //DMA channel 4 Interrupt
    def_irq_handler    DMA0_Channel5_IRQHandler            //DMA channel 5 Interrupt
    def_irq_handler    DMA0_Channel6_IRQHandler            //DMA channel 6 Interrupt
    def_irq_handler    DMA0_Channel7_IRQHandler            //DMA channel 7 Interrupt
    def_irq_handler    DMA0_Channel8_IRQHandler            //DMA channel 8 Interrupt
    def_irq_handler    DMA0_Channel9_IRQHandler            //DMA channel 9 Interrupt
    def_irq_handler    DMA0_Channel10_IRQHandler           //DMA channel 10 Interrupt
    def_irq_handler    DMA0_Channel11_IRQHandler           //DMA channel 11 Interrupt
    def_irq_handler    DMA0_Channel12_IRQHandler           //DMA channel 12 Interrupt
    def_irq_handler    DMA0_Channel13_IRQHandler           //DMA channel 13 Interrupt
    def_irq_handler    DMA0_Channel14_IRQHandler           //DMA channel 14 Interrupt
    def_irq_handler    DMA0_Channel15_IRQHandler           //DMA channel 15 Interrupt
    def_irq_handler    PORTA_IRQHandler                    //Port A pin detect interrupt
    def_irq_handler    PORTB_IRQHandler                    //Port B pin detect interrupt
    def_irq_handler    PORTC_IRQHandler                    //Port C pin detect interrupt
    def_irq_handler    PORTD_IRQHandler                    //Port D pin detect interrupt
    def_irq_handler    PORTE_IRQHandler                    //Port E pin detect interrupt
    def_irq_handler    UART0_IRQHandler                    //UART0 Transmit / Receive Interrupt
    def_irq_handler    UART1_IRQHandler                    //UART1 Transmit / Receive Interrupt
    def_irq_handler    UART2_IRQHandler                    //UART2 Transmit / Receive Interrupt
    def_irq_handler    UART3_IRQHandler                    //UART3 Transmit / Receive Interrupt
    def_irq_handler    SPI0_IRQHandler                     //SPI0 Interrupt
    def_irq_handler    SPI1_IRQHandler                     //SPI1 Interrupt
    def_irq_handler    SPI2_IRQHandler                     //SPI2 Interrupt
    def_irq_handler    I2C0_IRQHandler                     //I2C0 Interrupt
    def_irq_handler    EIO_IRQHandler                      //EIO Interrupt
    def_irq_handler    CAN0_IRQHandler                     //CAN0 Interrupt
    def_irq_handler    CAN0_Wakeup_IRQHandler              //CAN0 Wakeup Interrupt
    def_irq_handler    CAN1_IRQHandler                     //CAN1 Interrupt
    def_irq_handler    CAN1_Wakeup_IRQHandler              //CAN1 Wakeup Interrupt
    def_irq_handler    CAN2_IRQHandler                     //CAN2 Interrupt
    def_irq_handler    CAN2_Wakeup_IRQHandler              //CAN2 Wakeup Interrupt
    def_irq_handler    CAN3_IRQHandler                     //CAN3 Interrupt
    def_irq_handler    CAN3_Wakeup_IRQHandler              //CAN3 Wakeup Interrupt
    def_irq_handler    PDT0_IRQHandler                     //PDT0 Interrupt
    def_irq_handler    PDT1_IRQHandler                     //PDT1 Interrupt
    def_irq_handler    ADC0_IRQHandler                     //ADC0 Interrupt
    def_irq_handler    ADC1_IRQHandler                     //ADC1 Interrupt
    def_irq_handler    ACMP0_IRQHandler                    //ACMP0 Interrupt
    def_irq_handler    WDG_IRQHandler                      //WDG Interrupt
    def_irq_handler    EWDG_IRQHandler                     //EWDG Interrupt
    def_irq_handler    MCM_IRQHandler                      //MCM Interrupt
    def_irq_handler    LVD_IRQHandler                      //LVD Interrupt
    def_irq_handler    SPM_IRQHandler                      //SPM Interrupt
    def_irq_handler    RCM_IRQHandler                      //RCM Interrupt
    def_irq_handler    PWM0_Overflow_IRQHandler            //PWM0 Overflow Interrupt
    def_irq_handler    PWM0_Channel_IRQHandler             //PWM0 Channel Interrupt
    def_irq_handler    PWM0_Fault_IRQHandler               //PWM0 Fault Interrupt
    def_irq_handler    PWM1_Overflow_IRQHandler            //PWM1 Overflow Interrupt
    def_irq_handler    PWM1_Channel_IRQHandler             //PWM1 Channel Interrupt
    def_irq_handler    PWM1_Fault_IRQHandler               //PWM1 Fault Interrupt
    def_irq_handler    PWM2_Overflow_IRQHandler            //PWM2 Overflow Interrupt
    def_irq_handler    PWM2_Channel_IRQHandler             //PWM2 Channel Interrupt
    def_irq_handler    PWM2_Fault_IRQHandler               //PWM2 Fault Interrupt
    def_irq_handler    PWM3_Overflow_IRQHandler            //PWM3 Overflow Interrupt
    def_irq_handler    PWM3_Channel_IRQHandler             //PWM3 Channel Interrupt
    def_irq_handler    PWM3_Fault_IRQHandler               //PWM3 Fault Interrupt
    def_irq_handler    PWM4_Overflow_IRQHandler            //PWM4 Overflow Interrupt
    def_irq_handler    PWM4_Channel_IRQHandler             //PWM4 Channel Interrupt
    def_irq_handler    PWM4_Fault_IRQHandler               //PWM4 Fault Interrupt
    def_irq_handler    PWM5_Overflow_IRQHandler            //PWM5 Overflow Interrupt
    def_irq_handler    PWM5_Channel_IRQHandler             //PWM5 Channel Interrupt
    def_irq_handler    PWM5_Fault_IRQHandler               //PWM5 Fault Interrupt
    def_irq_handler    RTC_IRQHandler                      //RTC Interrupt
    def_irq_handler    PCT_IRQHandler                      //PCT Interrupt
    def_irq_handler    TIMER_Channel0_IRQHandler           //TIMER Channel0 Interrupt
    def_irq_handler    TIMER_Channel1_IRQHandler           //TIMER Channel1 Interrupt
    def_irq_handler    TIMER_Channel2_IRQHandler           //TIMER Channel2 Interrupt
    def_irq_handler    TIMER_Channel3_IRQHandler           //TIMER Channel3 Interrupt
    def_irq_handler    CSE_IRQHandler                      //CSE Interrupt
    def_irq_handler    FLASH_ECC_IRQHandler                //Flash ECC Interrupt
    def_irq_handler    FLASH_IRQHandler                    //Flash Interrupt
    def_irq_handler    FLASH_Collision_IRQHandler          //Flash Collision Interrupt
    def_irq_handler    ECC_1BIT_IRQHandler                 //ECC 1BIT Interrupt
    def_irq_handler    ECC_2BIT_IRQHandler                 //ECC 2BIT Interrupt

DMA0_Channel0_IRQHandler:
    @ IRQ Handler - mark that we received interrupt
    movs r0, #0xAA         @ Marker value
    bx lr

SysTick_Handler:
    @ SysTick Handler - mark that we received SysTick  
    movs r0, #0xBB         @ Marker value
    bx lr

    .end
