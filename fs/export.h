#ifndef _file_h_
#define _file_h_

struct filestat {
    uint16_t type;
    uint32_t size;
    uint16_t linkcnt;
};

int fs_init(int n);
int fs_unlink(char *path);
int fs_link(char *new, char *old);
int fs_mknode(char *path, uint16_t type);

int fileopen(char *path, uint16_t mode);
int fileseek(int fd, uint32_t off);
int filewrite(int fd, void *buf, int sz);
int fileread(int fd, void *buf, int sz);
int filestat(int fd, struct filestat *st);
int fileclose(int fd);

#endif
