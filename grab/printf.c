#include <types.h>
#include <vga.h>
#include <arg.h>
#include <util.h>

static char *digits = "0123456789abcdef";

static int _char(char c) {
    putchar(c);
    return 1;
}

static int _string(char *s) {
    int l = 0;
    // Cap the max string length to 1K
    // The rest of the string would be dropped!
    while (*s && l < (1<<10)) {
        putchar(*s++);
        l++;
    }
    return l;
}

static int _int(int x, int sign, int base) {
    // max signed int is 2147483647
    // max unsigned int is 4294967295
    // both are 10 digits and even shorter if base 16 is used
    if (!x) {
        putchar('0');
        return 1;
    }
    uint32_t abs;
    char xx[11];
    int i, d;
    abs = !sign ? x : (!(x == 1 << 31) ? (x > 0 ? x : -x) : (1 << 31));
    for (i = 0; abs > 0; abs /= base) {
        d = abs % base;
        xx[i++] = digits[d];
    }
    int n = i;
    if (sign && x < 0)
        putchar('-');
    while (--i >= 0)
        putchar(xx[i]);
    return n;
}

static int _two(int x) {
    int n = 0;
    for (; x; x >>= 1, n++)
        putchar(x & 1 ? '1' : '0');
    return n;
}

// Supports %d %x %p %c %s
// Additionally supports width specifiers:
// %10d constructs an output space that's 10 characters wide
// and prints the integer at the beginning, padding spaces to fill up the space
void printf(char *s, ...) {
    va_list ap;
    va_start(ap, s);
    enum { SPECIFIER, REGULAR, WIDTH_SPECIFIER, TYPE_SPECIFIER } state = REGULAR;
    while (*s) {
        char widbuf[10]; // Stores the withd specifier if used
        char *widbuf_ptr; // Where we are in 'widbuf?'
        int n; // Number of characters printed by the type specifier handling functions - _char(), _string(), and _int()
                // Used to determine how many space left in width for padding spaces from behind
                // Note. We do not support front padding!
        switch (state) {
        case REGULAR:
            if (*s == '%')
                state = SPECIFIER;
            else
                putchar(*s);
            s++;
            break;
        case SPECIFIER:
            if (isdigit(*s)) {
                state = WIDTH_SPECIFIER;
                widbuf_ptr = widbuf;
            } else if (*s)
                state = TYPE_SPECIFIER;
            break;
        case WIDTH_SPECIFIER:
            if (isdigit(*s) && (widbuf_ptr - widbuf) < 10)
                *widbuf_ptr++ = *s++;
            else {
                state = TYPE_SPECIFIER;
                *widbuf_ptr = 0;
            }
            break;
        case TYPE_SPECIFIER:
            switch (*s++)
            {
            case 'c':
                n = _char(va_arg(ap, char));
                break;
            case 's':
                n = _string(va_arg(ap, char*));
                break;
            case 'd':
                n = _int(va_arg(ap, int), 1, 10);
                break;
            case 'u':
                n = _int(va_arg(ap, int), 0, 10);
                break;
            case 'x':
                n = _int(va_arg(ap, int), 0, 16);
                break;
            case 'p':
                putchar('0');
                putchar('x');
                n = _int(va_arg(ap, int), 0, 16) + 2;
                break;
            case 't':
                n = _two(va_arg(ap, int));
            default:
                break;
            }

            if (widbuf_ptr != widbuf) {
                int wid = str2uint(widbuf);
                int pad = wid - n;
                for (int i = 0; i < pad; i++)
                    putchar(' ');
                widbuf_ptr = widbuf;
            }

            state = REGULAR;
            break;
        }
    }
    va_end(ap);
}
