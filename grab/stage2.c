#include <types.h>
#include <vga.h>
#include <pci.h>
#include <printf.h>
#include <panic.h>
#include <ide.h>
#include <fs.h>
#include <assert.h>
#include <inc.h>

void start2(int pcimod) __attribute__((section(".text.start2")));

void do_ls(char *path) {
    uint32_t inum = fs_lookup(path);
    if (inum == NULLINUM) {
        printf("ls: %s: No such file or directory\n", path);
        return;
    }
    uint32_t off = 0;
    for (;;) {
        struct dirent d;
        uint32_t n = inode_read(inum, &d, sizeof d, off);
        if (!n)
            break;
        if (d.inum)
            printf("%s\n", d.name);
        off += sizeof d;
    }
}

void start2(int pcimod) {
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
        panic("ide: channl 1: pci native mode: unsupported");
    if (!(h.progif & (1 << 2)))
        printf("ide: channel 2: compatibility mode\n");
    else
        panic("ide: channl 2: pci native mode: unsupported");
    if ((h.progif & (1 << 7)))
        printf("ide: DMA supported\n");
    ide_init();
    ide_list();
    ide_sel(0);
    struct partition *partitions = ide_get_partitions(0);
    assert(!fs_init(&partitions[0], ide_read, ide_write, (printfunc)printf));
    do_ls("/");
}
