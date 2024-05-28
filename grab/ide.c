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
    int p;
    int mbr;
    struct partition ptbl[4];
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
#define PORT_DRIVE_HEAD 0x6
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
static uint32_t lba_to_chs(int lba)
{
    uint32_t c = (lba / 63) / 255;
    uint32_t h = (lba / 63) % 255;
    uint32_t s = lba % 63 + 1;
    return (c | h << 16 | s << 24);
}

// Check the presence of 'drive' on 'channel'
// Drive can only be either 0 or 1
static int drive_present(int channel, int drive)
{
    int base = !channel ? PRIMARY_BASE : SECONDARY_BASE;
    // Ask the IDE controller to select the specified channel and drive
    outb(0xa0 | (drive << 4), base + PORT_DRIVE_HEAD);
    for (int i = 0; i < 1000; i++)
        // Read the status register 1000 times
        // We should at least see some bits getting set during this
        if (inb(base + PORT_STATUS))
            return 1;
    return 0;
}

static void info_drive(int channel, int drive)
{
    int drivenum = drive + channel + (channel ? 1 : 0);
    if (!ide[channel][drive].p)
        return;
    if (!ide[channel][drive].mbr) {
        printf("(hd%d) ", drivenum);
        return;
    }
    for (int i = 0; i < 4; i++)
        if (ide[channel][drive].ptbl[i].nsectors)
            printf("(hd%d, msdos%d) ", drivenum, i);
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
    uint32_t chs = lba_to_chs(n);
    outb(1, base + PORT_SECCNT);
    outb((uint8_t)(chs >> 24), base + PORT_SECTOR);
    outb((uint8_t)(chs), base + PORT_SYLLOW);
    outb((uint8_t)(chs >> 8), base + PORT_SYLHIGH);
    outb(0xa0 | (drive << 4) | (uint8_t)(chs >> 16), base + PORT_DRIVE_HEAD);
    if (w) {
        outb(CMD_WRSECT, base + PORT_COMMAND);
        for (int i = 0; i < BLOCKSIZE / 4; i++) {
            while ((status = inb(base + PORT_STATUS) & (STATUS_READY | STATUS_BUSY)) != STATUS_READY);
            outl(((uint32_t *)buf)[i], base + PORT_DATA);
        }
    } else {
        outb(CMD_RDSECT, base + PORT_COMMAND);
        for (int i = 0; i < BLOCKSIZE / 4; i++) {
            while ((status = inb(base + PORT_STATUS) & (STATUS_READY | STATUS_BUSY)) != STATUS_READY);
            ((uint32_t *)buf)[i] = inl(base + PORT_DATA);
        }
    }
}

static void init_drive(int channel, int drive) {
    union block b;
    ide[channel][drive].p = 1;
    ide_rw_abs(0, &b, 0, channel, drive);
    // Check for the MBR magic
    if (*(uint16_t *)&b.bytes[510] != 0xaa55) {
        ide[channel][drive].mbr = 0;
        return;
    }
    ide[channel][drive].mbr = 1;
    // Read the partition table from the MBR
    struct partition *p = (struct partition *)(&b.bytes[510] - sizeof (*p) * 4);
    for (int i = 0; i < 4; i++, p++)
        ide[channel][drive].ptbl[i] = *p;
}

// Users of the IDE module have to specifiy their selection
// of the channel and drive before any IDE functions can
// be called to avoid undetermined behavior.
void ide_sel(int channel, int drive) {
    drive_sel = drive;
    channel_sel = channel;
}

// Return the partition based on the *current* channel and drive selection.
void ide_get_partition(int num, struct partition *p) {
    *p = ide[channel_sel][drive_sel].ptbl[num];
}

// Prob all drives a
void ide_init()
{
    // Check the presence of drives on each IDE channel
    for (int channel = 0; channel < 2; channel++)
        for (int drive = 0; drive < 2; drive++)
            if (drive_present(channel, drive))
                init_drive(channel, drive);
}

// Prob IDE drives and record their presence and partitions
void ide_list() {
    // Check the presence of drives on each IDE channel
    for (int channel = 0; channel < 2; channel++)
        for (int drive = 0; drive < 2; drive++) {
                info_drive(channel, drive);
                printf(" ");
            }
    printf("\n");
}

static void ide_rw(int n, void *buf, int w) 
{
    ide_rw_abs(n, buf, w, channel_sel, drive_sel);
}

void ide_write(int n, void *buf)
{   
    ide_rw(n, buf, 1);
}
 
void ide_read(int n, void *buf)
{
    ide_rw(n, buf, 0);
}
