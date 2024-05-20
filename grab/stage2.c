#include <vga.h>

void start2(void) __attribute__((section(".text.start2")));

void start2() {
    for (;;) {
        for (int i = 0; i < 26; i++)
            putchar('a' + i);
        putchar('\n');
    }
}
