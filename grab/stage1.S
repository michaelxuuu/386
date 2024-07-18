#include <asm.h>

.text
.code16

.global _start
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

// set up stack at 0x9fc00 = 0x90000 + 0xfc00
        mov $0xfc00, %sp
        mov %sp, %bp

// set vga mode and clear screen
        mov $0, %ah
        mov $3, %al
        int $0x10

// later, we will need drive id bios has saved in %dl for us
// but before that, we will definitely use %dl for other purposes.
// therefore, we save it 
        mov %dx, boot_drive

// enable a20 line so that we'll be able to access odd megabyte addresses
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
        mov boot_drive, %dx
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

// BIOS "SMAP" (System Map) function (int 0x15, EAX=e820h) writes the system address map to ES:DI
// in the form of an array of address range descriptors.
// Each descriptor is 20 bytes (5 longs), or 24 bytes (6 longs) in the ACPI 3.0 version:
// long1: base low
// long2: base high
// long3: length low
// long4: length high
// long5: type
// long6: ACPI 3.0 extended attributes (ignored here, QEMU SeaBIOS returns the 20-byte version)
// 
// To get the map and pass it to stage2, we have ES:DI initialized to 0x8000:0 and place the descriptors
// returned by BIOS E820 calls sequentially starting there. Then we save the address 0x80000 to 'smap_ptr',
// which later will be pushed onto the stack as an argument to start2() defined in stage2.c, along with
// 'smap_len' indicating the length (the number of descriptors in the map).
	    // "SMAP" function requires such a continuation number. Must be initialized to 0 by user 
	    // and set and returned by BIOS. When BIOS sets it to 0, it means no more descriptors to be read.
	    xor %ebx, %ebx
	    // Set es:di to 0x8000:0
	    mov $0x8000, %ax
	    mov %ax, %es
	    xor %di, %di
smap_loop:
	    mov $0xe820, %eax      // BIOS call number
	    mov $24, %ecx    	   // Buffer size - each descriptor is 20 bytes
	    mov $0x0534D4150, %edx // BIOS requires we write the hex representation of "SMAP" (System Map) into EDX
	    int $0x15
	    cmp %edx, %eax
	    jne die     // Error detection: EAX is set to the "SMAP" signature if successful
	    jc die      // Error detection: Carry flag is clear if successful
	    incl smap_len
	    add $24, %di
	    cmp $0, %ebx      // End of list condition: EBX being 0 indicates the end of list is reached
	    jne smap_loop
smap_done:
	    movl $0x80000, smap_ptr

// use bios to get the pci configuration space access mode (1 or 2)
        xor %al, %al
        mov $0xb101, %ax
        int $0x1a
        mov %ax, pci_access_mode

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
        ljmpl $8, $0x90000+go32
        // From here to when we finish go32
        // we should not perform any data access
        // that involves %ds and %ss!!!

.code32
go32:
        mov $16, %ax
        mov %ax, %ds
        mov %ax, %ss
        // adjust sp to 0x90000 + sp as we did with %cs in ljmpl!!!
        add $0x90000, %esp
        add $0x90000, %ebp
        // jump tp 0x0 where we placed the bootloader kernel
	    pushl 0x90000+smap_len
	    pushl 0x90000+smap_ptr
	    pushl 0x90000+pci_access_mode
	    xor %eax, %eax
        call *%eax
        jmp die

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
// This routie uses BIOS functions and should not be used
// after the BIOS IVT area gets overwritten.
print:
        push %si
        mov $0x0e, %ah // select bios routine (only once)
print_loop:
        mov (%si), %al
        // has 0 been reached?
        cmp $0, %al
        je print_loop
        // print char in %al
        int $0x10
        // increment pointer
        inc %si
        jmp print_loop
        pop %si
        ret

// boot drive id
boot_drive:
	    .long 0

// memory map length - #address range descriptors in the system address map returned by bios e820:
smap_len:
	    .long 0

// address of the first descriptor in the address map
smap_ptr:
	    .long 0

// pci configuration space access mode
pci_access_mode:
	    .long 0

gdt:
// null
        .word 0, 0, 0, 0
        GDTE(0xfffff, 0, 1)
        GDTE(0xfffff, 0, 0)

gdt_ptr:
        .word gdt_ptr - gdt - 1
        .long gdt+0x90000 // must add 0x90000 gdt_ptr stores the linear address of the gdt at this stage

disk_err_msg:
        .string "disk error"

        .fill 510-(.-_start),1,0
        .byte 0x55
        .byte 0xaa

