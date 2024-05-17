#include "../kernel/fs.h"
#include "../fs/export.h"

#include <time.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/stat.h>
#include <sys/wait.h>

int vhd_fd;

void disk_write(int n, void *buf)
{
    assert(lseek(vhd_fd, n * BLOCKSIZE, SEEK_SET) == n * BLOCKSIZE);
    assert(write(vhd_fd, buf, BLOCKSIZE) == BLOCKSIZE);
}

void disk_read(int n, void *buf)
{
    assert(lseek(vhd_fd, n * BLOCKSIZE, SEEK_SET) == n * BLOCKSIZE);
    assert(read(vhd_fd, buf, BLOCKSIZE) == BLOCKSIZE);
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
    int fd;
    if (!path)
        return;
    if ((fd = fileopen(path, O_RDONLY)) == -1) {
        fprintf(stderr, "fileopen failed\n");
        return;
    }
    struct dirent de;
    while (fileread(fd, &de, sizeof de)) {
        if (!de.inum)
            continue;
        printf("%s\n", de.name);
    }
    assert(!fileclose(fd));
}

static void cmd_mkdir(char *path) {
    if (fs_mknode(path, T_DIR)) {
        fprintf(stderr, "filemkdir failed\n");
        return;
    }
}

static void cmd_migrate(char *mypath, char *hostpath) {
    int hostfd = open(hostpath, O_RDONLY, 0644);
    if (hostfd < 0) {
        fprintf(stderr, "%s not found in host fs\n", hostpath);
        return;
    }
    int myfd;
    if (fs_mknode(mypath, T_REG)) {
        fprintf(stderr, "failed to create %s in myfs\n", mypath);
        return;
    }
    assert((myfd = fileopen(mypath, O_WRONLY)) >= 0);
    char c;
    for (;;) {
        int n;
        assert((n = read(hostfd, &c, 1)) >= 0);
        if (!n)
            break;
        assert(filewrite(myfd, &c, 1));
    }
    assert(close(hostfd) >= 0);
    assert(fileclose(myfd) >= 0);
}

static void cmd_retrieve(char *hostpath, char *mypath) {
    int hostfd = open(hostpath, O_CREAT | O_TRUNC | O_WRONLY , 0644);
    int myfd = fileopen(mypath, O_RDONLY);
    if (hostfd < 0) {
        perror("host open");
        return;
    }
    if (myfd < 0) {
        fprintf(stderr, "%s not found in myfs\n", mypath);
        close(hostfd);
        return;
    }
    char c;
    for (;;) {
        int n;
        assert((n = fileread(myfd, &c, 1)) >= 0);
        if (!n)
            break;
        assert(write(hostfd, &c, 1));
    }
    assert(close(hostfd) >= 0);
    assert(fileclose(myfd) >= 0);
}

static void cmd_touch(char *path) {
    if (fs_mknode(path, T_REG)) {
        fprintf(stderr, "fs_mknode failed\n");
        return;
    }
}

static void cmd_stat(char *path) {
    int fd;
    if ((fd = fileopen(path, O_WRONLY)) < 0) {
        fprintf(stderr, "fileopen failed\n");
        return;
    }
    struct filestat st;
    assert(filestat(fd, &st) >= 0);
    printf("type:%d\nsize:%d\nlinkcnt:%d\n", st.type, st.size, st.linkcnt);
}

static void cmd_write(char *path, u32 off, u32 sz, char *words) {
    int fd;
    if ((fd = fileopen(path, O_WRONLY)) < 0) {
        fprintf(stderr, "fileopen failed\n");
        return;
    }
    assert(fileseek(fd, off) >= 0);
    assert(filewrite(fd, words, strlen(words)) == strlen(words));
    assert(fileclose(fd) >= 0);
}

static void cmd_read(char *path, u32 off, u32 sz) {
    int fd;
    if ((fd = fileopen(path, O_RDONLY)) < 0) {
        fprintf(stderr, "fileopen failed\n");
        return;
    }
    char buf[BLOCKSIZE];
    assert(fileseek(fd, off) >= 0);
    int n = fileread(fd, buf, sz);
    if (n < 0) {
        printf("fileread failed\n");
        assert(fileclose(fd) >= 0);
        return;
    }
    for (int i = 0; i < n; i++) {
        if (isprint(buf[i]))
            printf("%c", buf[i]);
        else if (!buf[i])
            printf("\\0");
        else
            printf("\\?");
    }
    puts("");
    assert(fileclose(fd) >= 0);
}

int fs_init(int partition_num);

#define CMDLEN 32
int main(int argc, char *argv[]) 
{
    if (argc < 3) {
        fprintf(stderr, "usage: main <vhd_name> <partition_num>\n");
        exit(1);
    }

    if ((vhd_fd = open(argv[1], O_RDWR)) < 0) {
        perror("open");
        exit(1);
    }

    int n = atoi(argv[2]);
    if (strlen(argv[2]) != 1 || 
        !isnumber(argv[2][0]) ||
        n < 1 ||
        n > 4) {
        fprintf(stderr, "invalid partition number\n");
        exit(1);
    }

    fs_init(n);

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
            if (cnt < 5)
                fprintf(stdout, "write: write <path> <off> <size> <words>\n");
            else
                cmd_write(args[1], atoi(args[2]), atoi(args[3]), args[4]);
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
