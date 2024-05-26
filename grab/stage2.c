#include <types.h>
#include <vga.h>
#include <pci.h>
#include <printf.h>
#include <panic.h>
#include <kbd.h>


void start2(int pcimod) __attribute__((section(".text.start2")));

void start2(int pcimod) {
    printf("probing pci devices...\n");
    pci_prob_dev(0);
    pci_list();
    printf("checking ide controller...\n");
    int ide = pci_get_dev(0x8086, 0x7010);
    if (ide == -1)
        panic("no ide controller detected");
    struct pcihdr h;
    pci_read_hdr(ide, &h);
    if (!(h.progif & (1 << 0)))
        printf("ide: channel 1: compatibility mode\n");
    else
        panic("ide: channl 1: pci native mode: unsupported");
    if (!(h.progif & (1 << 2)))
        printf("ide: channel 2: compatibility mode\n");
    else
        panic("ide: channl 2: pci native mode: unsupported");
    if ((h.progif & (1 << 7)))
        printf("ide: DMA supported\n");

    // kbd polling test
    while(1) printf("%c", kbd_test_polling());
}
