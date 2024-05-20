.global _start
.text

#include "../kernel/include/asm.h"

.code16
_start:
// relocate boot sector from 0x7c00 to 0x90000
    // we use "rep movsw" that repeats moving one word from %ds:%si to %es:%di %cx times
    mov $256, %cx // 512 bytes are 256 words
    // set %ds:%si to 0x7c0:0
    mov $0x7c0, %ax
    mov %ax, %ds
    xor %si, %si
    // set %es:%di to 0x9000:0
    mov $0x9000, %ax
    mov %ax, %es
    xor %di, %di
    // start copying
    rep movsw
// jump to 0x90000:go16 and start exeucting from there
// note. this requires linking the code to 0 instead of 0x7c00
    ljmp $0x9000, $go16
go16:
// disable interrupts
    cli
// initialize segment registers
    mov $0x9000, %ax
    mov %ax, %ds
    mov %ax, %ss
// set up stack at 0x9000
    mov $0x9000, %sp
    mov %sp, %bp
// set vga mode and clear screen
    mov $0, %ah
    mov $3, %al
    int $0x10
// later, we will need drive id bios has saved in %dl for us
// but before that, we will definitely use %dl for other purposes.
// therefore, we save it on the stack
    push %dx
// set a20 line so that we'll be able to access odd megabyte addresses
    call ibf8042
    mov $0xd1, %al
    out %al, $0x64
    call ibf8042
    mov $0xdf, %al
    out %al, $0x60
    call ibf8042
// read off the bootloader kernel image from the disk
// bootloader kernel image is assumed to start at the seocnd block and take up 32k in size
// read sectors [2, 63]
    // set %es:%bx to 0x10000 
    pop %dx
    mov $0x1000, %ax
    mov %ax, %es
    mov $0, %bx
    mov $0, %ch // C
    mov $0, %dh // H
    mov $2, %cl // S
    mov $62, %al // #sectors
    mov $2, %ah
    int $0x13
    jc die
// read sector 64
    // set %es:%bx to 0x17c00
    mov $0x7c00, %bx
    mov $0, %ch // C
    mov $1, %dh // H
    mov $1, %cl // S
    mov $1, %al // #sectors
    mov $2, %ah
    int $0x13
    jc die
// now we're done with bios and can overwrite its memroy!
// move the bootloader kernel image to the zero address
// relocate 63 sectors starting from 0x10000 to 0x0
    push %ds
    mov $(63 * 256), %cx
    // %ds:%si -> 0x1000:0
    mov $0x1000, %ax
    mov %ax, %ds
    xor %si, %si
    // %ds:%si -> 0:0
    mov $0, %ax
    mov %ax, %es
    xor %di, %di
    rep movsw
    pop %ds
// enter protected mode
to32: // debugging symbol
    lgdt gdt_ptr
    mov %cr0, %eax
    or $1, %eax
    mov %eax, %cr0
d:
    ljmpl $8, $0x90000+go32
.code32
go32:
    mov $16, %ax
    mov %ax, %ds
    mov %ax, %ss
    // jump tp 0x0 where we placed the bootloader kernel
    pushl $0
    ret

// test the input buffer full (ibf) bit is set in i8042's status register
// ibf indicates an error response from i8042 in a20 enabling sequence
ibf8042:
    in $0x64, %al
    test $2, %al
    jnz die
    ret

die:
    jmp .

// print the string at %si
print:
    push %si
    mov $0x0e, %ah // select bios routine (only once)
print.1:
    mov (%si), %al
    // has 0 been reached?
    cmp $0, %al
    je print.1
    // print char in %al
    int $0x10
    // increment pointer
    inc %si
    jmp print.1
print.2:
    pop %si
    ret

gdt:
// null
    .word 0, 0, 0, 0
    GETE(0xfffff, 0, 1)
    GETE(0xfffff, 0, 0)

gdt_ptr:
    .word gdt_ptr - gdt - 1
    .long gdt+0x90000 // must add 0x90000 gdt_ptr stores the linear address of the gdt at this stage

disk_err_msg:
    .string "disk error"

    .fill 510-(.-_start),1,0
    .byte 0x55
    .byte 0xaa
