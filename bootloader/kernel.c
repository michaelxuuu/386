#include "../fs.h"
#include "../types.h"

#include <assert.h>
#include <string.h>

void fs_init(const char *vhd);
u32 alloc_inode(u16 type);
int free_inode(u32 n);
int read_inode(u32 n, struct dinode *p);
int write_inode(u32 n, struct dinode *p);
u32 inode_read(u32 n, void *buf, u32 sz, u32 off);
u32 inode_write(u32 n, void *buf, u32 sz, u32 off);


