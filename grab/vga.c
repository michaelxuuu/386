#include <types.h>
#include <asm.h>
#include <vga.h>
#include <util.h>
#include <pio.h>
#include <panic.h>

// crtc i/o ports
#define PORT_INDEX      0x3d4
#define PORT_DATA       0x3d5

// crtc internal registers
#define IDX_CURSOR_CTRL     10  // cursor control register
#define IDX_CURSOR_LOC_LOW  15  // holds lower 8 bits of the cursor position
#define IDX_CURSOR_LOC_HIGH 14  // holds higher 8 bits of the cursor position

#define CURSOR_CTRL_EN (1 << 5)

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
uint16_t *textbuf = (uint16_t *)0xb8000;

// Read an internal register
static uint8_t crtc_read(uint8_t index) 
{
    // Pick an internal register by writing its index into the index port
    outb(index, PORT_INDEX);
    // Read from the data port
    return inb(PORT_DATA);
}

// Write an internal register
static void crtc_write(uint8_t data, uint8_t index) 
{
    outb(index, PORT_INDEX);
    outb(data, PORT_DATA);
}

// Return the current cursor position as a linear offset in the 80x25 pixel array
uint16_t vga_get_cursor() 
{
    return crtc_read(IDX_CURSOR_LOC_LOW) | crtc_read(IDX_CURSOR_LOC_HIGH) << 8;
}

void vga_set_cursor(uint16_t off) 
{
    if (off < 0 || off >= 80*25)
        panic("vga_set_cursor: Invalid offset");
    crtc_write(off, IDX_CURSOR_LOC_LOW);
    crtc_write(off >> 8, IDX_CURSOR_LOC_HIGH);
}

// Clear text (preserve color) and set cursor to offset 0
void vga_reset() 
{
    for (int i = 0; i < 25*80; textbuf[i++] &= 0xff00);
    vga_set_cursor(0);
}

void vga_putchar(uint16_t off, uint8_t c, uint8_t color)
{
    if (off < 0 || off >= 80*25)
        panic("vga_putchar: Invalid offset");
    if (!color)
        color = (BGND_BLACK | FGND_WHITE);
    textbuf[off] = (c | color << 8);
}

uint16_t vga_scroll(uint16_t off)
{
    if (off < 25*80)
        return off;
    memcpy(&textbuf[0], &textbuf[80], 24*80*2);
    for (int i = 24*80; i < 25*80; textbuf[i++] &= 0xff00);
    return 24*80;
}

void vga_set_color(int row, uint8_t color)
{
    if (row < 0 || row >= 25)
        panic("vga_set_color: Invalid row number\n");
    uint16_t off = row * 80;
    for (int i = 0; i < 80; i++, off++) {
        textbuf[off] &= 0x00ff;
        textbuf[off] |= (color << 8);
    }
}

void vga_hide_cursor()
{
    crtc_write(crtc_read(IDX_CURSOR_CTRL) | CURSOR_CTRL_EN, IDX_CURSOR_CTRL);
}

void vga_show_cursor()
{
    crtc_write(crtc_read(IDX_CURSOR_CTRL) & ~CURSOR_CTRL_EN, IDX_CURSOR_CTRL);
}

