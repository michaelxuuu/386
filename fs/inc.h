#include "../kernel/fs.h"
#include "../kernel/types.h"
#include "../partition/partition.h"

#include <assert.h>
#include <string.h>

#define NFILES 16
#define MAXPATH 64

// Defined in fs.c
extern struct superblock su;

// inode.c
int free_inode(u32 n);
u32 alloc_inode(u16 type);
int read_inode(u32 n, struct dinode *p);
int write_inode(u32 n, struct dinode *p);
u32 inode_read(u32 n, void *buf, u32 sz, u32 off);
u32 inode_write(u32 n, void *buf, u32 sz, u32 off);

// fs.c
int fs_init(int n);
int fs_unlink(char *path);
int fs_link(char *new, char *old);
u32 fs_lookup(char *path, int parent);
u32 dir_lookup(u32 inum, char *name, u32 *poff);

// util.c
char *getname(char *path, char *name, char *parent);

// User of this fs implemention pl must provide definitions for the below two functions
// in other C files compiled along with fs.c
extern void disk_write(int n, void *buf);
extern void disk_read(int n, void *buf);
