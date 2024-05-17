#include "../kernel/fs.h"
#include "../partition/partition.h"

#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <assert.h>

int fd;

#define min(x, y) (x < y ? x : y)

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

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: grab <vhd_name> <root_partition>\n");
        exit(1);
    }

    if ((fd = open(argv[1], O_RDWR)) < 0) {
        perror("open");
        exit(1);
    }
    union block b;
    int n = atoi(argv[2]);
    if (strlen(argv[2]) != 1 || !isnumber(argv[2][0]) || n < 1 || n > 4) {
        fprintf(stderr, "invalid root partition number\n");
        exit(1);
    }
    --n;
    disk_read(0, &b); // Read the first sector
    if (!ismbr(&b)) { // Check if the first sector is the mbr from whom we need the partition table
        fprintf(stderr, "missing MBR\n");
        exit(1);
    }
    // Obtain partition table from the mbr read
    struct par partble[4];
    struct par *p = getpar(&b, 0);
    for (int i = 0; i < 4; partble[i++] = *p++);
    if (!partble[n].sysid) {// Is the specified partition used?
        fprintf(stderr, "partition %d is empty", n + 1);
        exit(1);
    }
    // Compute the number of sectors reserved for the boot sector
    int min_start_lba = partble[n].startlba;
    for (int i = 1; i < 4; i++)
        if (partble[i].sysid)
            min_start_lba = min(min_start_lba, partble[i].startlba);
    // Check if the partitioning reserved enough space for grab to install its kernel image
    if (min_start_lba < 65) {
        fprintf(stderr, "#boot sectors %d is fewer than required %d\n", min_start_lba - 1, 63);
        exit(1);
    }
    int stage1, stage2;
    if ((stage1 = open("./grab/stage1.bin", O_RDONLY)) < 0 ||
        (stage2 = open("./grab/stage2.bin", O_RDONLY)) < 0) {
            perror("open");
            exit(1);
    }
    char buf[BLOCKSIZE];
// Install grab 2 in the post-mbr gap
    for (int i = 1; i < 64; i++) {
        assert(read(stage2, buf, BLOCKSIZE) == BLOCKSIZE);
        disk_write(i, buf);
    }
// Install grab 1
    assert(read(stage1, buf, BLOCKSIZE) == BLOCKSIZE);
    // Mark the specified partition as bootable
    partble[n].bootable = 0x80;
    // Copy the partition table
    p = getpar((union block *)buf, 0);
    for (int i = 0; i < 4; *p++ = partble[i++]);
    disk_write(0, buf);
    close(fd);
    close(stage1);
    close(stage2);
    return 0;
}
