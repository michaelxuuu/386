#include "include/types.h"
#include "include/asm.h"
#include "include/vga.h"
#include "include/util.h"
#include "include/pio.h"
// crtc i/o ports
#define PORT_INDEX      0x3d4
#define PORT_DATA       0x3d5
// crtc internal registers
#define IDX_CURSOR_CTRL     10  // cursor control register
#define IDX_CURSOR_LOC_LOW  15  // holds lower 8 bits of the cursor position
#define IDX_CURSOR_LOC_HIGH 14  // holds higher 8 bits of the cursor position
// vga text buffer
// Mapped to physical memory 0xb8000
// Contains 80x25 words as {color, ascii} pairs
// Color formation:
// Bit 76543210
//     ||||||||
//     |||||^^^-fg colour
//     ||||^----fg colour bright bit
//     |^^^-----bg colour
//     ^--------bg colour bright bit OR enables blinking Text
#define COLOR (BGND_BLACK | FGND_WHITE)
uint16_t *textbuf = (uint16_t *)0xb8000;
// Read an internal register
uint8_t crtc_read(uint8_t index) {
    // Pick an internal register by writing its index into the index port
    outb(index, PORT_INDEX);
    // Read from the data port
    return inb(PORT_DATA);
}
// Write an internal register
void crtc_write(uint8_t data, uint8_t index) {
    outb(index, PORT_INDEX);
    outb(data, PORT_DATA);
}
// Return the current cursor position as a linear offset in the 80x25 pixel array
uint16_t get_cursor() {
    return crtc_read(IDX_CURSOR_LOC_LOW) | crtc_read(IDX_CURSOR_LOC_HIGH) << 8;
}
void set_cursor(uint16_t off) {
    crtc_write(off, IDX_CURSOR_LOC_LOW);
    crtc_write(off >> 8, IDX_CURSOR_LOC_HIGH);
}
// Print one char at the current cursor position and scroll if necessary
void putchar(char c) {
    uint16_t off = get_cursor();
    switch (c)
    {
    case '\n':
    case '\r':
        off = off + 80 - off % 80;
        break;
    case '\b':
        off--;
        break;
    case '\t':
        off += 4;
    default:
        textbuf[off++] = (c | COLOR << 8);
        break;
    }
    // off-- may cause it to become negative. But we do not support scroll-up.
    // So we reset off to 0 if it becomes negative.
    if (off < 0)
        off = 0;
    // Any increment of off may cause it to overflow the text buffer.
    // We must scroll down in case of overflow.
    // Luckily, we always scroll down by 1 row.
    if (off >= 25*80) {
        off = 24*80;
        memcpy(&textbuf[0], &textbuf[80], 24*80);
    asm("d:");
        for (int i = 24*80; i < 25*80; textbuf[i++] &= 0xff00);
    }
    set_cursor(off);
}
// Clear text (preserve color) and set cursor to offset 0
void vga_reset() {
    for (int i = 0; i < 25*80; textbuf[i] &= 0xff00);
    set_cursor(0);
}
