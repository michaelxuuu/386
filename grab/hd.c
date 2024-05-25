//
// Simple Hard Disk Driver
//
// - Assume ATA drive
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

// Primary fixed disk i/o ports
#define PORT_DATA       0x1f0
#define PORT_ERR        0x1f1
#define PORT_SECCNT     0x1f2
#define PORT_SECTOR     0x1f3
#define PORT_SYLLOW     0x1f4
#define PORT_SYLHIGH    0x1f5
#define PORT_DRIVE_HEAD 0x1f6
#define PORT_STATUS     0x1f7
#define PORT_COMMAND    PORT_STATUS

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

static void disk_write(int n, void *buf)
{
    
}

static void disk_read(int n, void *buf)
{
}
