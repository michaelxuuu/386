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

#define min(x, y) (x < y ? x : y)
#define max(x, y) (x > y ? x : y)

struct {
    int fd;
} mkfs;

static void panic(char *s) {
    printf("%s", s);
    for(;;);
}

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

// Find the next word in a null-terminated string.
// Return a null pointer when there are no more words left.
// Return a pointer to the char after the current word.
// To get all the words in a string, keep calling this function
// in a loop and use the return of the previous call as the 's'
// argument of the next call until 0 is returned.
static char *nextword(char *s, char word[]) {
    for (; *s && isspace(*s); s++);
    if (!*s)
        return 0;
    int i = 0;
    for (; *s && !isspace(*s); s++, i++)
        word[i] = *s;
    word[i] = 0;
    return s;
}

void do_ls(char *s) {
    char path[64];
    if (!nextword(s, path)) {
        printf("usage: ls <path>");
        return;
    }
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

void do_migrate(char *s) {
    char paths[2][64];
    for (int i = 0; i < 2; i++) {
        if (!(s = nextword(s, paths[i]))) {
            printf("usage: migrate <host_path> <path>\n");
            return;
        }
    }
    uint32_t inum = fs_lookup(paths[1]);
    if (inum != NULLINUM) {
        printf("migrate: %s: File exists\n", paths[1]);
        return;
    }
    inum = fs_mknod(paths[1], T_REG);
    if (inum == NULLINUM)
        panic("fs error");
    int fd = open(paths[0], O_RDWR);
    if (fd < 0) {
        perror("open");
        return;
    }
    uint32_t off = 0;
    for (;;) {
        char c;
        int n = read(fd, &c, 1);
        if (!n)
            break;
        int nn = inode_write(inum, &c, 1, off++);
        if (nn != 1)
            panic("fs error!");
    }
    close(fd);
}

void do_retrieve(char *s) {
    char paths[2][64];
    for (int i = 0; i < 2; i++) {
        if (!(s = nextword(s, paths[i]))) {
            printf("usage: retrieve <path> <host_path>\n");
            return;
        }
    }
    uint32_t inum = fs_lookup(paths[0]);
    if (inum == NULLINUM) {
        printf("retrieve: %s: No such file or directory\n", paths[0]);
        return;
    }
    int fd = open(paths[1], O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
    if (fd < 0) {
        perror("open");
        return;
    }
    uint32_t off = 0;
    for (;;) {
        char c;
        int n = inode_read(inum, &c, 1, off++);
        if (!n)
            break;
        write(fd, &c, 1);
    }
    close(fd);
}

void do_read(char *s, int w) {
    char *p;
    char buf[64];
    char path[64];
    int sz;
    uint32_t off;
    if (!(s = nextword(s, path))) {
        printf("usage: read <path> <size> <offset>\n");
        return;
    }
    if (!(s = nextword(s, buf))) {
        printf("read: missing size\n");
        return;
    }
    sz = (uint32_t)strtol(buf, &p, 10);
    if (!p) {
        printf("read: %s: invalid size\n", buf);
        return;
    }
    if (!(s = nextword(s, buf))) {
        printf("read: missing offset\n");
        return;
    }
    off = (uint32_t)strtol(buf, &p, 10);
    if (!p) {
        printf("read: %s: invalid offset\n", buf);
        return;
    }
    uint32_t inum = fs_lookup(path);
    if (inum == NULLINUM) {
        printf("read: %s: No such file or directory\n", path);
        return;
    }
    if (w) {
        int n = inode_write(inum, buf, min(64, sz), off);
        if (n != min(64, sz))
            panic("fs error");
        return;
    }
    int n = inode_read(inum, buf, min(64, sz), off);
    for (int i = 0; i < n; i++) {
        if (isprint(buf[i]))
            printf("%c", buf[i]);
        else printf("%c\n", 0xFFFD);
    }
}

void do_mkdir(char *s) {
    char path[64];
    if (!nextword(s, path))
        printf("usage: mkdir <path>\n");
    fs_mknod(path, T_DIR);
}

void do_touch(char *s) {
    char path[64];
    if (!nextword(s, path))
        printf("usage: touch <path>\n");
    fs_mknod(path, T_REG);
}

void do_rm(char *s) {
    char path[64];
    if (!nextword(s, path))
        printf("usage: rm <path>\n");
    fs_unlink(path);
}

int main(int argc, char *argv[]) 
{
    if (argc < 3) {
        fprintf(stderr, "usage: main <vhd_name> <partition_num>\n");
        exit(1);
    }

    mkfs.fd = open(argv[1], O_RDWR);
    if (mkfs.fd < 0) {
        perror("open");
        exit(1);
    }

    int n = atoi(argv[2]);
    if (strlen(argv[2]) != 1 || !isnumber(argv[2][0]) || n < 1 || n > 4) {
        fprintf(stderr, "mkfs: %s: invalid partition number\n", argv[2]);
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
        char s[64], w[64];
        printf("> "), fflush(stdout);
        fgets(s, 64, stdin);
        s[strlen(s) - 1] = 0; // rid of '/n'
        char *p;
        if (!(p = nextword(s, w)))
            continue;
        if (!strncmp("ls", w, 2))
            do_ls(p);
        else if (!strncmp("migrate", w, 7))
            do_migrate(p);
        else if (!strncmp("retrieve", w, 8))
            do_retrieve(p);
        else if (!strncmp("read", w, 4))
            do_read(p, 0);
        else if (!strncmp("write", w, 5))
            do_read(p, 1);
        else if (!strncmp("mkdir", w, 5))
            do_mkdir(p);
        else if (!strncmp("touch", w, 5))
            do_touch(p);
        else if (!strncmp("rm", w, 2))
            do_rm(p);
        else if (!strncmp("quit", w, 4))
            exit(1);
        else
            printf("mkfs: %s: invalid command\n", w);
    }
}
