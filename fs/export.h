#ifndef _file_h_
#define _file_h_

#include "../kernel/types.h"

struct filestat {
    u16 type;
    u32 size;
    u16 linkcnt;
};

int fs_init(int n);
int fs_unlink(char *path);
int fs_link(char *new, char *old);
int fs_mknode(char *path, u16 type);

int fileopen(char *path, u16 mode);
int fileseek(int fd, u32 off);
int filewrite(int fd, void *buf, int sz);
int fileread(int fd, void *buf, int sz);
int filestat(int fd, struct filestat *st);
int fileclose(int fd);

#endif
