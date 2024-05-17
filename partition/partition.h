#ifndef _partition_h_
#define _partition_h_

#include "../kernel/fs.h"
#include "../kernel/types.h"

// Partition table entry
struct par {
    u8  bootable;
    u8  startc:8;
    u16 starth:10;
    u16 starts:6;
    u8  sysid;
    u8  endc:8;
    u16 endh:10;
    u16 ends:6;
    u32 startlba;
    u32 nsectors;
};

static inline int ismbr(union block *b)
{
    return *(u16 *)&b->bytes[510] == 0xaa55;
}

static inline struct par *getpar(union block *mbr, int n)
{
    return (struct par *)(&mbr->bytes[512] - 2 - sizeof(struct par) * 4);
}

#endif
