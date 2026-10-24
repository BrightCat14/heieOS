ENTRY(_start)
SECTIONS
{
    . = 0x00020000;
    .text   : { *(.text.boot) *(.text*) }  
    .rodata : { *(.rodata*) }
    .data   : { *(.data*) }
    .bss    : { *(.bss*) *(COMMON) }
    ASSERT(. <= 0x00021000, "image grew into the BROM's FEL area at 0x21000")
    ASSERT(SIZEOF(.bss) == 0, "no .bss support: initialise globals explicitly or use locals")
}
