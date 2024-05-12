.code16
.global _start
.text

_start:
// set vga mode and clear screen
    mov $0, %ah
    mov $3, %al
    int $0x10
// set up stack at 0x9000
    mov $0x9000, %sp
    mov %sp, %bp
// later, we will need drive id bios has saved in %dl for us
// but before that, we will definitely use %dl for other purposes.
// therefore, we save it on the stack
    push %dx
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
// jump to 0x90000:.1 and start exeucting from there
// note. this requires linking the code to 0 instead of 0x7c00
//     ljmp $0x9000, $go
// go:
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
    jc disk_err
// read sector 64
    // set %es:%bx to 0x17c00
    mov $0x7e00, %bx
    mov $0, %ch // C
    mov $1, %dh // H
    mov $1, %cl // S
    mov $1, %al // #sectors
    mov $2, %ah
    int $0x13
    jc disk_err

// jump to and execute the bootloader kernel at 0x10000
    ljmp $0x1000, $0


disk_err:
    mov $disk_err_msg, %si
    call print
    jmp .

// print the string at %si
print:
    push %si
    mov $0x0e, %ah // select bios routine (only once)
print.0:
    mov (%si), %al
    // has 0 been reached?
    cmp $0, %al
    je print.1
    // print char in %al
    int $0x10
    // increment pointer
    inc %si
    jmp print.0
print.1:
    pop %si
    ret

disk_err_msg:
    .string "disk error"

    .fill 510-(.-_start),1,0
    .byte 0x55
    .byte 0xaa
