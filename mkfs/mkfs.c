#include "../kernel/fs.h"

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
#include <sys/stat.h>

struct partition_table_entry {
    unsigned char stat;
    unsigned char start_chs[3];
    unsigned char type;
    unsigned char end_chs[3];
    unsigned int start_lba;
    unsigned int sector_cnt;
};

u32 alloc_inode(u16 type);

int fd;

void disk_write(int n, void *buf)
{
    assert(lseek(fd, n * BLOCKSIZE, SEEK_SET) == n * BLOCKSIZE);
    assert(write(fd, buf, BLOCKSIZE) == BLOCKSIZE);
}

void disk_read(int n, void *buf)
{
    assert(lseek(fd, n * BLOCKSIZE, SEEK_SET) == n * BLOCKSIZE);
    assert(read(fd, buf, BLOCKSIZE) == BLOCKSIZE);
}

void printsu(struct superblock *su) {
    printf("superblock:\n"
            "#inodes:%u\n"
            "#blocks(tot):%u\n"
            "#blocks(log):%u\n"
            "#blocks(ino):%u\n"
            "#blocks(dat):%u\n"
            "start(log):%u\n"
            "start(ino):%u\n"
            "start(bmp):%u\n"
            "start(dat):%u\n"
            "magic:%x\n",
            su->ninodes, 
            su->nblock_tot,
            su->nblock_log,
            su->nblock_inode, 
            su->nblock_dat,
            su->slog,
            su->sinode,
            su->sbitmap,
            su->sdata,
            su->magic
    );
}

void mkfs(int partition_num) {
    union block b;
    // Convert to 0-based indexing
    partition_num--;
    // Read mbr if there is one
    disk_read(0, &b);
    if (b.bytes[510] != 0x55 || b.bytes[511] != 0xaa) {
        fprintf(stderr, "mbr not found\n");
        exit(1);
    }
    // Read partition table and format the first partition
    struct partition_table_entry partition_table[4];
    for (int i = 0; i < 4; i++)
        partition_table[i] = *((struct partition_table_entry *)(&b.bytes[512] - sizeof(struct partition_table_entry) - 2) - (3-i));
    // Check if partition_num is valid
    if (!partition_table[partition_num].type) {
        fprintf(stderr, "partition %d is empty\n", partition_num+1);
        exit(1);
    }
    printf("partition info:\n"
            "lba start: %d\n"
            "sector count: %d\n"
            "type: 0x%x\n",
            partition_table[partition_num].start_lba,
            partition_table[partition_num].sector_cnt,
            partition_table[partition_num].type);
    // Read super block
    int sublock_num = partition_table[partition_num].start_lba;
    disk_read(sublock_num, &b);
    // The specified partition has already been formatted
    if (b.su.magic == FSMAGIC) {
        printf("partition %d has already been fromatted\n", partition_num+1);
        printsu(&b.su);
        return;
    }
    // Format the specified partition
    printf("formatting partition %d...\n", partition_num+1);
    // Format vhd
    // Zero the partition
    char buf[BLOCKSIZE] = {0};
    for (int i = partition_table[partition_num].start_lba; i < partition_table[partition_num].sector_cnt; i++)
        disk_write(i, buf);
    // Prep super block
    b.su.ninodes = NINODES;
    b.su.nblock_tot = partition_table[partition_num].sector_cnt;
    b.su.nblock_log = NBLOCKS_LOG;
    b.su.nblock_inode = NINODES / NINODES_PER_BLOCK;
    b.su.nblock_dat = b.su.nblock_tot - (NBLOCKS_LOG + b.su.nblock_inode + 1 + 1);
    b.su.ssuper = sublock_num;
    b.su.slog = sublock_num + 1;
    b.su.sinode = b.su.slog + NBLOCKS_LOG;
    b.su.sbitmap = b.su.sinode + b.su.nblock_inode;
    b.su.sdata = b.su.sbitmap + 1;
    b.su.magic = FSMAGIC;
    // Write super block to disk
    disk_write(b.su.ssuper, &b);
    printsu(&b.su);
    // Reserve inode 0 and 1
    alloc_inode(T_DIR);
    alloc_inode(T_DIR);
}

int main(int argc, char *argv[]) {

    if (argc < 3) {
        fprintf(stderr, "usage: mkfs <vhd_name> <partition_num>\n");
        exit(1);
    }

    if ((fd = open(argv[1], O_RDWR)) < 0) {
        perror("open");
        exit(1);
    }

    int partition_num = atoi(argv[2]);
    if (strlen(argv[2]) != 1 || 
        !isnumber(argv[2][0]) ||
        partition_num < 1 ||
        partition_num > 4) {
        fprintf(stderr, "invalid partition number\n");
        exit(1);
    }

    mkfs(partition_num);

    if (close(fd) < 0) {
        perror("close");
        exit(1);
    }

    return 0;
}
