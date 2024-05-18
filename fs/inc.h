#ifndef _file_h_
#define _file_h_

#include <stdint.h>
#include "../kernel/fs.h"
#include "../partition/partition.h"

typedef void (*diskfunc)(int blocknum, void *buf);
typedef void (*printfunc)(char *fmt, ...);

int fs_unlink(char *path);
int fs_link(char *new, char *old);
int fs_mknod(char *path, uint16_t type);
int fs_format(struct partition *p);
int fs_init(struct partition *p, diskfunc rfunc, diskfunc wfunc, printfunc pfunc);
uint32_t fs_lookup(char *path, int parent);
int read_inode(uint32_t n, struct dinode *p);
int write_inode(uint32_t n, struct dinode *p);
uint32_t inode_read(uint32_t n, void *buf, uint32_t sz, uint32_t off);
uint32_t inode_write(uint32_t n, void *buf, uint32_t sz, uint32_t off);

#endif
