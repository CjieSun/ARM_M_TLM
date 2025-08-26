/*
** ###################################################################
**     Processor:           AC7840x with 1024 KB pflash
**     Compiler:            GCC ARM Compiler
**
**     Abstract:
**         Linker file for the GCC ARM Compiler
**
**     AutoChips Inc. (C) 2023. All rights reserved.
**
**     Target      : Autochips AC784xx
**
**     Distribution: The file is distributed without any warranty
**                of any kind.
**
** ###################################################################
*/
/*OUTPUT_ARCH(arm)*/
ENTRY(Reset_Handler)

/* Specify the memory areas */
MEMORY
{
  /* PFlash */
  m_text                (RX)  : ORIGIN = 0x00000000, LENGTH = 0x00080000

  /* SRAM */
  m_data                (RW)  : ORIGIN = 0x20000000, LENGTH = 0x00010000

}

/* Define output sections */
SECTIONS
{
  /*Defaults*/
  HEAP_SIZE  =  0x00000000;
  STACK_SIZE =  0x00002000;

  /* If symbol __flash_vector_table__=1 is defined at link time
  * the interrupt vector will not be copied to RAM.
  * Warning: Using the interrupt vector from Flash will not allow
  * INT_SYS_InstallHandler because the section is Read Only.
  */
  M_VECTOR_RAM_SIZE =  0x0200;

  /* The program code and other data goes into internal flash */
  .text :
  {
    __Vectors = .;
    . = ALIGN(4);
    __stext = .;
    KEEP(*(.isr_vector))     /* Startup code */
    *(.text)                 /* .text sections (code) */
    *(.text*)                /* .text* sections (code) */
    . = ALIGN(4);
    *(.rodata)               /* .rodata sections (constants, strings, etc.) */
    *(.rodata*)              /* .rodata* sections (constants, strings, etc.) */
    KEEP(*(.eh_frame))
  } > m_text

  .ARM.exidx :
  {
    PROVIDE(__exidx_start = .);
    *(.ARM.exidx* .gnu.linkonce.armexidx.*)
    PROVIDE(__exidx_end = .);
  } > m_text
  __etext = .;             /* Define a global symbol at end of code. */

  .init :
  {
    . = ALIGN(4);
    KEEP(*(.init))
    KEEP(*(.init_array))
    KEEP(*(.init.*))
    . = ALIGN(4);
  } > m_text

   __DATA_ROM = .;           /* Symbol is used by startup for data initialization. */

  .data : AT(__DATA_ROM)
  {
    . = ALIGN(4);
    __DATA_RAM = .;
    __data_start__ = .;      /* Create a global symbol at data start. */
    KEEP(*(.data))           /* .data sections */
    KEEP(*(.data*))          /* .data* sections */
    . = ALIGN(4);
    __data_end__ = .;        /* Define a global symbol at data end. */
  } > m_data

  __DATA_END = __DATA_ROM + (__data_end__ - __data_start__);
  __CODE_ROM = __DATA_END;   /* Symbol is used by code initialization. */

  .code : AT(__CODE_ROM)
  {
    . = ALIGN(4);
    __CODE_RAM = .;
    __code_start__ = .;      /* Create a global symbol at code start. */
    __code_ram_start__ = .;
    *(.code_ram)             /* Custom section for storing code in RAM */
    . = ALIGN(4);
    __code_end__ = .;        /* Define a global symbol at code end. */
    __code_ram_end__ = .;
  } > m_data

  __CODE_END = __CODE_ROM + (__code_end__ - __code_start__);

  /* Uninitialized data section. */
  .bss :
  {
    /* This is used by the startup in order to initialize the .bss section. */
    . = ALIGN(4);
    __BSS_START = .;
    __bss_start__ = .;
    *(.bss)
    *(.bss*)
    *(COMMON)
    . = ALIGN(4);
    __bss_end__ = .;
    __BSS_END = .;
  } > m_data

  .heap :
  {
    . = ALIGN(8);
    __end__ = .;
    __heap_start__ = .;
    PROVIDE(end = .);
    PROVIDE(_end = .);
    PROVIDE(__end = .);
    __HeapBase = .;
    . += HEAP_SIZE;
    __HeapLimit = .;
    __heap_limit = .;
    __heap_end__ = .;
  } > m_data

  .stack :
  {
    . = ALIGN(8);
    __StackBottom = .;
    . += STACK_SIZE;
    __StackTop = .;
  } > m_data

  PROVIDE(__stack = __StackTop);

  __eSRAM_L = .;
}
