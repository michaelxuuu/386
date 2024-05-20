#include "vga.h"

void start2() {
    for (;;) {
        for (int i = 0; i < 26; i++)
            putchar('a' + i);
        putchar('\n');
    }
}
