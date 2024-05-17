#ifndef _partition_h_
#define _partition_h_

#include <stdint.h>

// Partition table entry
struct partition {
    uint8_t  bootable;
    uint8_t  startc:8;
    uint16_t starth:10;
    uint16_t starts:6;
    uint8_t  sysid;
    uint8_t  endc:8;
    uint16_t endh:10;
    uint16_t ends:6;
    uint32_t startlba;
    uint32_t nsectors;
};

#endif
