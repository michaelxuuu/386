#include <vga.h>
#include <arg.h>
#include <util.h>
#include <types.h>

static char *digits = "0123456789abcdef";

static void _char(char c) {
    putchar(c);
}

static void _string(char *s) {
    while (*s)
        putchar(*s++);
}

static void _int(int x, int sign, int base) {
    // max signed int is 2147483647
    // max unsigned int is 4294967295
    // both are 10 digits and even shorter if base 16 is used
    uint32_t abs;
    char xx[11];
    int i, d;
    abs = !sign ? x : (!(x == 1 << 31) ? (x > 0 ? x : -x) : (1 << 31));
    for (i = 0; abs > 0; abs /= base) {
        d = abs % base;
        xx[i++] = digits[d];
    }
    if (sign && x < 0)
        putchar('-');
    while (--i >= 0)
        putchar(xx[i]);
}

void printf(char *s, ...) {
    va_list ap;
    va_start(ap, s);
    enum { PLACEHOLDER, REGULAR } state = REGULAR;
    for (; *s; s++) {
        if (state == REGULAR) {
            if (*s == '%')
                state = PLACEHOLDER;
            else
                putchar(*s);
        } else if (state == PLACEHOLDER) {
            switch (*s)
            {
            case 'c':
                _char(va_arg(ap, char));
                break;
            case 's':
                _string(va_arg(ap, char*));
                break;
            case 'd':
                _int(va_arg(ap, int), 1, 10);
                break;
            case 'u':
                _int(va_arg(ap, int), 0, 10);
                break;
            case 'x':
                _int(va_arg(ap, int), 0, 16);
                break;
            case 'p':
                putchar('0');
                putchar('x');
                _int(va_arg(ap, int), 0, 16);
                break;
            default:
                break;
            }
            state = REGULAR;
        }
    }
    va_end(ap);
}
