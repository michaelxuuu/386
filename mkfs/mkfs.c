#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include "../kernel/fs.h"
#include "../partition/partition.h"
#include "../fs/inc.h"

struct {
    int fd;
} mkfs;

static void disk_write(int n, void *buf)
{
    assert(lseek(mkfs.fd, n * BLOCKSIZE, SEEK_SET) == n * BLOCKSIZE);
    assert(write(mkfs.fd, buf, BLOCKSIZE) == BLOCKSIZE);
}

static void disk_read(int n, void *buf)
{
    assert(lseek(mkfs.fd, n * BLOCKSIZE, SEEK_SET) == n * BLOCKSIZE);
    assert(read(mkfs.fd, buf, BLOCKSIZE) == BLOCKSIZE);
}

static int arg_len(char *arg) {
    for (int i = 0; ; i++)
        if (!arg[i] || arg[i] == ' ')
            return i;
}

static char *get_arg(char *buf, int n) {
    int nn = 0, s = 0;
    for (int i = 0; buf[i]; i++) {
        // Start of an arg
        if (buf[i] != ' ' && !s) {
            if (nn++ == n)
                return &buf[i];
            s = !s;
        }
        // Start of spaces
        if (buf[i] == ' ' && s)
            s = !s;
    }
    return 0;
}

static int parse_args(char *buf, char *args[], int argct) {
    int i = 0;
    int cnt = 0;
    // Get 'argct' args or fewer if not as many
    for (; i < argct; i++)
        if (!(args[i] = get_arg(buf, i)))
            break;
    cnt = i;
    // Null-terminate the args in buf[].
    // Do this after since nulling buf elements interferes get_arg()
    for (i-- ; i >= 0; i--)
        args[i][arg_len(args[i])] = 0;
    return cnt;
}

static void cmd_ls(char *path) {
    uint32_t inum;
    uint32_t off = 0;
    if (!path)
        return;
    if ((inum = fs_lookup(path, 0)) == NULLINUM)
        return;
    struct dirent de;
    while (inode_read(inum, &de, sizeof de, off)) {
        if (!de.inum)
            continue;
        printf("%s\n", de.name);
        off += sizeof de;
    }
}

static void cmd_mkdir(char *path) {
    fs_mknod(path, T_DIR);
}

static void cmd_migrate(char *mypath, char *hostpath) {
    int hostfd = open(hostpath, O_RDONLY, 0644);
    if (hostfd < 0) {
        perror("host open");
        return;
    }
    if (fs_mknod(mypath, T_REG))
        return;
    char c;
    uint32_t inum;
    uint32_t off = 0;
    assert((inum = fs_lookup(mypath, 0)) != NULLINUM);
    for (;;) {
        int n;
        assert((n = read(hostfd, &c, 1)) >= 0);
        if (!n)
            break;
        assert(inode_write(inum, &c, 1, off++) == 1);
    }
    assert(close(hostfd) >= 0);
}

static void cmd_retrieve(char *hostpath, char *mypath) {
    int hostfd = open(hostpath, O_CREAT | O_TRUNC | O_WRONLY , 0644);
    uint32_t inum;
    if ((inum = fs_lookup(mypath, 0)) == NULLINUM)
        return;
    if (hostfd < 0) {
        perror("open");
        return;
    }
    char c;
    uint32_t off = 0;
    for (;;) {
        int n;
        assert((n = inode_read(inum, &c, 1, off++)) >= 0);
        if (!n)
            break;
        assert(write(hostfd, &c, 1));
    }
    assert(close(hostfd) >= 0);
}

static void cmd_touch(char *path) {
    fs_mknod(path, T_REG);
}

static void cmd_stat(char *path) {
    uint32_t inum;
    if ((inum = fs_lookup(path, 0)) == NULLINUM)
        return;
    struct dinode di;
    read_inode(inum, &di);
    printf("linkcnt:%u major:%u minor:%u size:%u type:%u\n", di.linkcnt, di.major, di.minor, di.size, di.type);
}

static void cmd_write(char *path, uint32_t off, char *s) {
    uint32_t inum;
    if ((inum = fs_lookup(path, 0)) == NULLINUM)
        return;
    assert(inode_write(inum, s, strlen(s), off) == strlen(s));
}

static void cmd_read(char *path, uint32_t off, uint32_t sz) {
    uint32_t inum;
    if ((inum = fs_lookup(path, 0)) == NULLINUM)
        return;
    char buf[BLOCKSIZE];
    uint32_t n = inode_read(inum, buf, sz, off);
    if (n != (uint32_t)-1)
        return;
    for (int i = 0; i < n; i++) {
        if (isprint(buf[i]))
            printf("%c", buf[i]);
        else if (!buf[i])
            printf("\\0");
        else
            printf("\\?");
    }
    puts("");
}

#define CMDLEN 32
int main(int argc, char *argv[]) 
{
    if (argc < 3) {
        fprintf(stderr, "usage: main <vhd_name> <partition_num>\n");
        exit(1);
    }

    if ((mkfs.fd = open(argv[1], O_RDWR)) < 0) {
        perror("open");
        exit(1);
    }

    int n = atoi(argv[2]);
    if (strlen(argv[2]) != 1 || !isnumber(argv[2][0]) || n < 1 || n > 4) {
        fprintf(stderr, "invalid partition number\n");
        exit(1);
    }

    union block b;
    // Read the first sector and assume it's the mbr
    disk_read(0, &b);
    if (*(uint16_t *)&b.bytes[510] != 0xaa55) {
        fprintf(stderr, "missing MBR\n");
        exit(1);
    }
    // Retrieve the partition table from the mbr
    struct partition partble[4];
    struct partition *p = (struct partition *)(&b.bytes[512] - 2 - sizeof(struct partition) * 4);
    for (int i = 0; i < 4; partble[i++] = *p++);
    // Does the specified partition number 'n' points to a valid partition with a non-zero 'sysid'
    if (!partble[n - 1].sysid) {
        fprintf(stderr, "partition %d is empty", n);
        exit(1);
    }
    
    if (fs_init(&partble[n - 1], disk_read, disk_write, printf) < 0) {
        fs_format(&partble[n - 1]);
        assert(fs_init(&partble[n - 1], disk_read, disk_write, printf) >= 0);
    }
    for (;;) {
        char cmd[CMDLEN];
        printf("> "), fflush(stdout);
        fgets(cmd, CMDLEN, stdin);
        cmd[strlen(cmd) - 1] = 0;
        char *args[5];
        int cnt = parse_args(cmd, args, 5);
        if (!cnt)
            continue;
        if (!strncmp(args[0], "ls", 2)) {
            if (cnt < 2)
                fprintf(stdout, "usage: ls <path>\n");
            else
                cmd_ls(args[1]);
        } else if (!strncmp(args[0], "mkdir", 5)) {
            if (cnt < 2)
                fprintf(stdout, "usage: mkdir <path>\n");
            else
                cmd_mkdir(args[1]);
        } else if (!strncmp(args[0], "migrate", 7)) {
            if (cnt < 3)
                fprintf(stdout, "usage: migrate <filepath> <host_path>\n");
            else
                cmd_migrate(args[1], args[2]);
        } else if (!strncmp(args[0], "retrieve", 8)) {
            if (cnt < 3)
                fprintf(stdout, "usage: retrieve <host_path> <filepath>\n");
            else
                cmd_retrieve(args[1], args[2]);
        } if (!strncmp(args[0], "read", 4)) {
            if (cnt < 4)
                fprintf(stdout, "read: read <path> <off> <size>\n");
            else
                cmd_read(args[1], atoi(args[2]), atoi(args[3]));
        } if (!strncmp(args[0], "write", 5)) {
            if (cnt < 4)
                fprintf(stdout, "write: write <path> <off> <word>\n");
            else
                cmd_write(args[1], atoi(args[2]), args[3]);
        } if (!strncmp(args[0], "stat", 4)) {
            if (cnt < 2)
                fprintf(stdout, "stat: stat <path>\n");
            else
                cmd_stat(args[1]);
        } if (!strncmp(args[0], "touch", 5)) {
            if (cnt < 2)
                fprintf(stdout, "touch: touch <path>\n");
            else
                cmd_touch(args[1]);
        } else if (!strncmp(args[0], "quit", 4))
            exit(0);
    }
}
