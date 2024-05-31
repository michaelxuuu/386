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

struct ide_drive {
    int exist;
    int msdos;
    int max_c;
    int max_h;
    struct partition partitions[4];
};

// IBM 5170 has 2 IDE channels, each supporting up 2 drives
// yielding 4 drives in total
static struct {
    struct ide_drive drives[4];
    int drive_sel;
} ide;

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

// Used internally to read and write drives based on *explicit* channel and drive selections ("abs"). 
// Unlike ide_rw(), which reads the current drive and channel selection from 'drive_sel' and 'channel_sel' set by ide_sel(), 
// this function requires explicit drive and channel selections.
static int ide_rw_abs(int drive, int c, int h, int s, void *buf, int w)
{
    uint8_t status;
    int base = drive < 2 ? PRIMARY_BASE : SECONDARY_BASE;
    drive &= 1;
    // Wait until the controller is not busy.
    while (inb(base + PORT_STATUS) & STATUS_BUSY);
    // Do not test the error bit here!
    // Because "the next command reset the error bit."
    // Otherwise, it will always return -1 here if there has been an error.
    // And the error bit will be left set forever!
    h &= 0x0f;
    s &= 0xff;
    c &= 0xffff;
    outb(1, base + PORT_SECCNT);
    outb((uint8_t)s, base + PORT_SECTOR);
    outb((uint8_t)c, base + PORT_SYLLOW);
    outb((uint8_t)(c >> 8), base + PORT_SYLHIGH);
    outb((uint8_t)0xa0 | (uint8_t)(drive << 4) | (uint8_t)h, base + PORT_SEL);
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
        // Check the busy bit first. It's being set indicates the controller is exeucting the command.
        // When this bit is set no other bit is valid in the status register!
        while ((status = inb(base + PORT_STATUS)) & STATUS_BUSY);
        // Now the controller has done executing the command. We can check for errors.
        if (status & STATUS_ERR)
            return -1;
        // Check if the controller's ready to do a read operation.
        while (((status = inb(base + PORT_STATUS)) & (STATUS_READY | STATUS_SEEK)) \
            != (STATUS_READY | STATUS_SEEK));
        // Do it!
        insl(base + PORT_DATA, buf, BLOCKSIZE / 4);
    }
    return 0;
}

static int ide_rw(int c, int h, int s, void *buf, int w)
{
    struct ide_drive *drive = &ide.drives[ide.drive_sel];
    if (c >= 0 || c < drive->max_c ||
        h >= 0 || h < drive->max_h ||
        s >= 1 || s < 63)
        return ide_rw_abs(ide.drive_sel, c, h, s, buf, w);
    printf("request (%d,%d,%d) out of bounds (%d,%d,%d)\n", c, h, s, \
            drive->max_c, \
            drive->max_h, \
            63);
    return -1;
}

// Prob IDE drives and record their presence and partitions
void ide_init()
{
    // Check the presence of drives on each IDE channel
    for (int drivenum = 0; drivenum < 4; drivenum++) {
        union block b;
        int max_c = 0;
        int max_h = 0;
        struct partition *p = \
            (struct partition *)(&b.bytes[510] - (sizeof (*p) << 2));
        int base = drivenum < 2 ? PRIMARY_BASE : SECONDARY_BASE;
        int retry_cnt = 1000;
        struct ide_drive *drive = &ide.drives[drivenum];
        // Does this drive exist?
        outb(0xa0 | ((drivenum & 1) << 4), base + PORT_SEL);
        for (; retry_cnt > 0 && !inb(base + PORT_STATUS); retry_cnt--);
        if (!retry_cnt)
            continue;
        // Probe the drive geometry
        for (; ide_rw_abs(drivenum, max_c, 0, 1, &b, 0) >= 0 \
            && max_c < 1024 + 1; max_c++);
        for (; ide_rw_abs(drivenum, 0, max_h, 1, &b, 0) >= 0 \
            && max_h < 16; max_h++);
        if (max_c > 1024) {
            printf("failed to probe geometry");
            max_c = -1;
            max_h = -1;
        }
        // Is this an msdos partitioned drive?
        ide_rw_abs(drivenum, 0, 0, 1, &b, 0);
        if (*(uint16_t *)&b.bytes[510] == 0xaa55) {
            drive->msdos = 1;
            // Read the partition table from the MBR.
            for (int i = 0; i < 4; i++, p++)
                drive->partitions[i] = *p;
        }
        drive->exist = 1;
        drive->max_c = max_c;
        drive->max_h = max_h;
    }
}

// Users of the IDE module have to specifiy their selection
// of the channel and drive before any IDE functions can
// be called to avoid undetermined behavior.
// "drivenum" can be 1, 2, 3, and 4.
int ide_sel(int drivenum) {
    if (!ide.drives[drivenum].exist)
        return -1;
    ide.drive_sel = drivenum & 1;
    return 0;
}

// Return a pointer to the partition table of the currrent drive
// based on the *current* channel and drive selection.
struct partition *ide_get_partitions() {
    if (!ide.drives[ide.drive_sel].msdos)
        return 0;
    return ide.drives[ide.drive_sel].partitions;
}

int ide_write(int c, int h, int s, void *buf)
{
    return ide_rw(c, h, s, buf, 1);
}
 
int ide_read(int c, int h, int s, void *buf)
{
    return ide_rw(c, h, s, buf, 0);
}

static void lba_to_chs(int lba, int *c, int *h, int *s)
{
    int max_h = ide.drives[ide.drive_sel].max_h;
    *s = lba % 63 + 1;
    *c = (lba / 63) / max_h;
    *h = (lba / 63) % max_h;
}

int ide_write_lba(int lba, void *buf)
{
    int c, h, s;
    lba_to_chs(lba, &c, &h, &s);
    return ide_write(c, h, s, buf);
}

int ide_read_lba(int lba, void *buf)
{
    int c, h, s;
    lba_to_chs(lba, &c, &h, &s);
    return ide_read(c, h, s, buf);
}
