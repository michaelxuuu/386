#include <types.h>
#include <vga.h>
#include <pci.h>
#include <printf.h>

void start2(int pcimod) __attribute__((section(".text.start2")));

void start2(int pcimod) {
    scan(0);
}
