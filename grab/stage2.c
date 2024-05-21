#include <vga.h>
#include <printf.h>

void start2(int pcimod) __attribute__((section(".text.start2")));

void start2(int pcimod) {
    printf("%d", 129);
}
