/* Unnecessary. Only needed if the program loaded by a loader. */
ENTRY(_start)

MEMORY
{
    /* Kernel is linked to 3G, aka the higher half kernel */
    ALL (rxw) : ORIGIN = 0xC0000000, LENGTH = 512K
}

/* To suppress warning: has a LOAD segment with RWX permissions */
PHDRS
{
    text PT_LOAD FLAGS(5);   /* 5 = 1 (R) | 4 (X) */
    rodata PT_LOAD FLAGS(4); /* 4 = 4 (R) */
    data PT_LOAD FLAGS(6);   /* 6 = 2 (W) | 4 (R) */
}

SECTIONS
{
    /* Place the .text section with RX permissions */
    .text :
    {
        /* This makes sure the start2() function gets placed at the begining of the output binary */
        *(.text._start)
        *(.text)
    } > ALL :text

    . = ALIGN(4);

    /* Place the .rodata section with R permissions */
    .rodata :
    {
        *(.rodata)
    } > ALL :rodata

    . = ALIGN(4);

    /* Place the .data section with RW permissions */
    .data :
    {
        *(.data)
    } > ALL :data

    . = ALIGN(4);

    /* Place the .bss section with RW permissions */
    .bss :
    {
        *(.bss)
    } > ALL :data
}
