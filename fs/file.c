// File abstraction level
// Here users see files and diretories instead of raw inodes

#include "inc.h"

struct filestat {
    u16 type;
    u32 size;
    u16 linkcnt;
};

// Open file structure
struct file {
    u32 off;
    u32 inum;
    u32 mode;
};

// Open files
static struct file files[NFILES];

int fileopen(char *path, u16 mode) {
    for (int i = 0; i < NFILES; i++) {
        if (files[i].inum == NULLINUM) {
            int inum = fs_lookup(path, 0);
            if (inum == NULLINUM)
                return -1;
            files[i].inum = inum;
            files[i].off = 0;
            files[i].mode = mode;
            return i;
        }
    }
    return -1;
}

int fileseek(int fd, u32 off) {
    if (files[fd].inum == NULLINUM)
        return -1;
    files[fd].off = off;
    return 0;
}

int filewrite(int fd, void *buf, int sz) {
    if (!files[fd].inum || !files[fd].mode)
        return -1;
    u32 n = inode_write(files[fd].inum, buf, sz, files[fd].off);
    files[fd].off += n;
    return n;
}

int fileread(int fd, void *buf, int sz) {
    if (!files[fd].inum || files[fd].mode & O_WRONLY)
        return -1;
    u32 n = inode_read(files[fd].inum, buf, sz, files[fd].off);
    files[fd].off += n;
    return n;
}

int fileclose(int fd) {
    if (files[fd].inum == NULLINUM) {
        return -1;
    }
    files[fd].inum = NULLINUM;
    return 0;
}

int filestat(int fd, struct filestat *st) {
    if (files[fd].inum == NULLINUM) {
        return -1;
    }
    struct dinode di;
    read_inode(files[fd].inum, &di);
    st->type = di.type;
    st->linkcnt = di.linkcnt;
    st->size = di.size;
    return 0;
}
