/* Linker script for ARMv6-M Thumb assembly tests */
/* Designed for ARM_M_TLM simulator */

MEMORY
{
    FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 256K
    RAM (rwx)  : ORIGIN = 0x20000000, LENGTH = 64K
}

ENTRY(_start)

SECTIONS
{
    /* Vector table */
    .vectors :
    {
        LONG(0x20010000)    /* Initial stack pointer (top of RAM) */
        LONG(_start + 1)    /* Reset vector (Thumb mode) */
        LONG(0)             /* NMI Handler */  
        LONG(0)             /* Hard Fault Handler */
        . = ALIGN(4);
    } > FLASH
    
    /* Code section */
    .text :
    {
        . = ALIGN(4);
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
        . = ALIGN(4);
    } > FLASH
    
    /* Initialized data */
    .data :
    {
        . = ALIGN(4);
        _sdata = .;
        *(.data)
        *(.data*)
        . = ALIGN(4);
        _edata = .;
    } > RAM AT > FLASH
    
    /* Uninitialized data */
    .bss :
    {
        . = ALIGN(4);
        _sbss = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
        . = ALIGN(4);
        _ebss = .;
    } > RAM
    
    /* Stack */
    .stack :
    {
        . = ALIGN(8);
        . = . + 1024;  /* 1KB stack */
        _estack = .;
    } > RAM
}

/* Provide stack top symbol */
_stack_top = ORIGIN(RAM) + LENGTH(RAM);