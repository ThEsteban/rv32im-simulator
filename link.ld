/* Tell the linker our entry point is the _start function */
ENTRY(_start)

/* Define our physical memory layout */
MEMORY {
    ram (rwx) : ORIGIN = 0x80000000, LENGTH = 128M
}

/* Map the C segments into the RAM block */
SECTIONS {
    /* Start outputting at 0x80000000 */
    . = 0x80000000;

    .text : {
        *(.init)
        *(.text .text.*)
    } > ram

    .rodata : {
        *(.rodata .rodata.*)
    } > ram

    .data : {
        *(.data .data.*)
    } > ram

    .bss : {
        *(.bss .bss.*)
    } > ram
}