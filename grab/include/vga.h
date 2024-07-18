/* Bits 3:0 are the foreground color, bits 6:4 are the
   background color, and bit 7 specifies blink (annoying). */
#define FGND_BLACK 0x0
#define FGND_BLUE  0x1
#define FGND_GREEN 0x2
#define FGND_CYAN  0x3
#define FGND_RED   0x4
#define FGND_MAG   0x5
#define FGND_BRWN  0x6
#define FGND_LGRAY 0x7 /* Light gray. */
#define FGND_DGRAY 0x8 /* Dark gray. */
#define FGND_BBLUE 0x9 /* Bright blue. */
#define FGND_BGRN  0xA /* Bright green. */
#define FGND_BCYAN 0xB /* Bright cyan. */
#define FGND_PINK  0xC
#define FGND_BMAG  0xD /* Bright magenta. */
#define FGND_YLLW  0xE
#define FGND_WHITE 0xF

#define BGND_BLACK 0x00
#define BGND_BLUE  0x10
#define BGND_GREEN 0x20
#define BGND_CYAN  0x30
#define BGND_RED   0x40
#define BGND_MAG   0x50
#define BGND_BRWN  0x60
#define BGND_LGRAY 0x70 /* Light gray. */

#define BLINK 0x80 /* Annoying. */

void vga_reset();
uint16_t vga_get_cursor();
void vga_set_cursor(uint16_t off);
void vga_clear(int start, int end);
void vga_putchar(uint16_t off, uint8_t c, uint8_t color);
uint16_t vga_scroll(uint16_t off);
void vga_set_color(int row, uint8_t color);
void vga_show_cursor();
void vga_hide_cursor();
