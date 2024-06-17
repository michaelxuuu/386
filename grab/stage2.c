#include <types.h>
#include <vga.h>
#include <pci.h>
#include <printf.h>
#include <panic.h>
#include <kbd.h>
#include <ide.h>
#include <assert.h>
#include <fs.h>
#include <fs-api.h>

struct addr_range_desc {
	uint32_t baselow;
	uint32_t basehigh;
	uint32_t lengthlow;
	uint32_t lengthhigh;
	uint32_t type;
	uint32_t acpi3;
};

void start2(int pcimod, struct addr_range_desc *mem_map, int mapsz) __attribute__((section(".text.start2")));

void shell();

void start2(int pcimod, struct addr_range_desc *mem_map, int mapsz) {
    printf("probing pci devices...\n");
    pci_prob_dev(0);
    pci_list();
    printf("checking ide controller...\n");
    int idedev = pci_get_dev(0x8086, 0x7010);
    if (idedev == -1)
        panic("no ide controller detected");
    struct pcihdr h;
    pci_read_hdr(idedev, &h);
    if (!(h.progif & (1 << 0)))
        printf("ide: channel 1: compatibility mode\n");
    else
        panic("ide: channl 1: pci native mode: unsupported\n");
    if (!(h.progif & (1 << 2)))
        printf("ide: channel 2: compatibility mode\n");
    else
        panic("ide: channl 2: pci native mode: unsupported\n");
    if ((h.progif & (1 << 7)))
        printf("ide: DMA supported\n");
    ide_init();
    shell();
}

