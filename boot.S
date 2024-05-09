
.code16
.global init16
.text

init16:
    mov $0, %ah /* func -> set video mode */

    .fill 510-(.-init16), 1, 0    /* add zeroes to make it 510 bytes long */
    .byte 0x55                /* magic bytes that tell BIOS that this is bootable */
    .byte 0xaa