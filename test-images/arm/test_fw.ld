/* Minimal linker script for arm_board (addresses: RM-VERIFY) */
MEMORY
{
    FLASH (rx)  : ORIGIN = 0x00400000, LENGTH = 8M
    DTCM  (rwx) : ORIGIN = 0x20000000, LENGTH = 128K
}

ENTRY(Reset_Handler)

SECTIONS
{
    .isr_vector : { KEEP(*(.isr_vector)) } > FLASH   /* VTOR = 0x00400000 */
    .text       : { *(.text*) *(.rodata*) } > FLASH
    .bss (NOLOAD) :
    {
        _sbss = .;
        *(.bss*) *(COMMON)
        _ebss = .;
    } > DTCM
    /DISCARD/ : { *(.data*) *(.ARM.exidx*) }
    _estack = ORIGIN(DTCM) + LENGTH(DTCM);
}
