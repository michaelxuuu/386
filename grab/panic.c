#include <printf.h>

void panic(char *s) {
    printf("panic: %s\n", s);
    for(;;);
}
