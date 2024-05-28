#include <types.h>
#include <pio.h>
#include <panic.h>
#include <fs.h>
#include <printf.h>

//
// Simple Hard Disk Driver
//
// - Non-interrupt (synchronous), meaning we don't queue the r/w requests
//   and rely on interrupts to know when the disk is ready - all disk r/w
//   calls are *blocking*. No I/O buffer cache implemented.
// - Non-DMA
// - Assume ISA
// - If PCI is used, assume the IDE controller operates in PCI legacy (compatibility mode)
//   where we assume fixed I/O ports rather than getting them from PCI config space BAR registers
//

// Error register bits
#define ERR_DAM     0x1
#define ERR_TRACK0  0x2
#define ERR_ABORT   0x4
#define ERR_ID      0x10
#define ERR_ECC     0x40
#define ERR_BAD     0x80

// IBM 5170 has 2 IDE channels, each supporting up 2 drives
// yielding 4 drives in total
static struct {
    int present;
    int ismsods;
    int nsector;
    struct partition partitions[4];
} ide[2][2];

static int drive_sel = 0;
static int channel_sel = 0;

// IDE controller i/o ports
#define PRIMARY_BASE    0x1f0
#define SECONDARY_BASE  0x170
#define PORT_DATA       0x0
#define PORT_ERR        0x1
#define PORT_SECCNT     0x2
#define PORT_SECTOR     0x3
#define PORT_SYLLOW     0x4
#define PORT_SYLHIGH    0x5
#define PORT_SEL 0x6
#define PORT_STATUS     0x7
#define PORT_COMMAND    PORT_STATUS

#define CHANNEL_PRIMARY PRIMARY_BASE
#define CHANNEL_SECONDARY SECONDARY_BASE

// Status register bits
#define STATUS_BUSY     0x80
#define STATUS_READY    0x40
#define STATUS_WRERR    0x20
#define STATUS_SEEK     0x10
#define STATUS_DRQ      0x8
#define STATUS_ECC      0x4
#define STTAUS_IDX      0x2
#define STATUS_ERR      0x1

// Command register values
#define CMD_RESTORE     0x10
#define CMD_SEEK        0x70
#define CMD_RDSECT      0x20
#define CMD_WRSECT      0x30
#define CMD_FORMAT      0x50
#define CMD_VERIFY      0x40
#define CMD_DIAGNOSE    0x90
#define CMD_SETPARAM    0x91

// Assume:
// 1024 cylinders
// 255 heads/tracks per cylinder
// 63 sectors per head/track
#define MAXLBA (255 * 63 * 1024)

// Convert the lba address to chs address
static uint32_t tochs(int lba)
{
    uint32_t c = (lba / 63) / 255;
    uint32_t h = (lba / 63) % 255;
    uint32_t s = lba % 63 + 1;
    return (c | h << 16 | s << 24);
}

// Used internally to read and write drives based on *explicit* channel and drive selections ("abs"). 
// Unlike ide_rw(), which reads the current drive and channel selection from 'drive_sel' and 'channel_sel' set by ide_sel(), 
// this function requires explicit drive and channel selections.
static void ide_rw_abs(int n, void *buf, int w, int channel, int drive)
{
    uint8_t status;
    int base = !channel ? PRIMARY_BASE : SECONDARY_BASE;
    // Wait until the controller is ready
    while ((status = inb(base + PORT_STATUS) & (STATUS_READY | STATUS_BUSY)) != STATUS_READY);
    // Has an error occured in the previous request we waited for?
    if ((status & (STATUS_ERR | STATUS_WRERR)))
        panic("Disk error!");
    uint32_t chs = tochs(n);
    outb(1, base + PORT_SECCNT);
    outb((uint8_t)(chs >> 24), base + PORT_SECTOR);
    outb((uint8_t)(chs), base + PORT_SYLLOW);
    outb((uint8_t)(chs >> 8), base + PORT_SYLHIGH);
    outb(0xa0 | (drive << 4) | (uint8_t)(chs >> 16), base + PORT_SEL);
    if (w) {
        outb(CMD_WRSECT, base + PORT_COMMAND);
        // The first sector may be written to the buffer *immediately* 
        // after the command has been sent, and data request is active.
        outsl(buf, base + PORT_DATA, BLOCKSIZE / 4);
    } else {
        outb(CMD_RDSECT, base + PORT_COMMAND);
        // Need to wait until the controller is ready!
        // This is usually done in the IRQ14 handler, but we can do it here
        // cus the CPU doesn't have much else to do anyway when running the bootloader.
        while ((status = inb(base + PORT_STATUS) & (STATUS_READY | STATUS_BUSY)) != STATUS_READY);
        insl(base + PORT_DATA, buf, BLOCKSIZE / 4);
    }
}

static void ide_rw(int n, void *buf, int w) 
{
    ide_rw_abs(n, buf, w, channel_sel, drive_sel);
}

// Prob IDE drives and record their presence and partitions
void ide_init()
{
    // Check the presence of drives on each IDE channel
    for (int x = 0; x < 4; x++) {
        union block b;
        struct partition *p = \
            (struct partition *)(&b.bytes[510] - (sizeof (*p) << 2));
        int channel = x >> 1;
        int drive = x & 1;
        int base = !channel ? PRIMARY_BASE : SECONDARY_BASE;
        int retry_cnt = 1000;
        int nsector = 0;
        outb(0xa0 | (drive << 4), base + PORT_SEL);
        for (; retry_cnt > 0 && !inb(base + PORT_STATUS); retry_cnt--);
        if (!retry_cnt)
            continue;
        ide[channel][drive].present = 1;
        ide[channel][drive].ismsods = 0;
        ide_rw_abs(0, &b, 0, channel, drive);
        if (*(uint16_t *)&b.bytes[510] != 0xaa55)
            continue;
        ide[channel][drive].ismsods = 1;
        for (int i = 0; i < 4; i++, p++) {
            ide[channel][drive].nsector += p->nsectors;
            ide[channel][drive].partitions[i] = *p;
        }
    }
}

void ide_list()
{
    for (int x = 0; x < 4; x++) {
        int channel = x >> 1;
        int drive = x & 1;
        if (!ide[channel][drive].present)
            continue;
        if (!ide[channel][drive].ismsods) {
            printf("(hd%d, ?)", x);
            continue;
        }
        for (int y = 0; y < 4; y++) {
            if (!ide[channel][drive].partitions[y].nsectors)
                continue;
            printf("(hd%d, msdos%d) ", x, y);
        }
    }
    printf("\n");
}

// Users of the IDE module have to specifiy their selection
// of the channel and drive before any IDE functions can
// be called to avoid undetermined behavior.
// "drivenum" can be 1, 2, 3, and 4.
void ide_sel(int drivenum) {
    drive_sel = drivenum & 1;
    channel_sel = drivenum >> 1;
}

// Return a pointer to the partition table of the currrent drive
// based on the *current* channel and drive selection.
struct partition *ide_get_partitions(int num) {
    return ide[channel_sel][drive_sel].partitions;
}

void ide_write(int n, void *buf)
{
    if (n >= ide[channel_sel][drive_sel].nsector)
        panic("Trying to read nonexistent sector");
    ide_rw(n, buf, 1);
}
 
void ide_read(int n, void *buf)
{
    if (n >= ide[channel_sel][drive_sel].nsector)
        panic("Trying to write nonexistent sector");
    ide_rw(n, buf, 0);
}
