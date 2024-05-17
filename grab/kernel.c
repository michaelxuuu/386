#include "../kernel/fs.h"
#include "../kernel/types.h"

#include <assert.h>
#include <string.h>

void fs_init(const char *vhd);
uint32_t alloc_inode(uint16_t type);
int free_inode(uint32_t n);
int read_inode(uint32_t n, struct dinode *p);
int write_inode(uint32_t n, struct dinode *p);
uint32_t inode_read(uint32_t n, void *buf, uint32_t sz, uint32_t off);
uint32_t inode_write(uint32_t n, void *buf, uint32_t sz, uint32_t off);


