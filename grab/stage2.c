#include <types.h>
#include <vga.h>
#include <pci.h>
#include <printf.h>

void start2(int pcimod) __attribute__((section(".text.start2")));

void start2(int pcimod) {
    printf("probing pci devices...\n");
    pci_scan(0);
    printf("checking ide controller...\n");
    printf("checking ide derives...\n");
}
