#include <stdint.h>
#include <assert.h>
#include <string.h>

#include "../kernel/fs.h"
#include "../partition/partition.h"

#define NFILES 16
#define MAXPATH 64

// Defined in fs.c
extern struct superblock su;

// inode.c
int free_inode(uint32_t n);
uint32_t alloc_inode(uint16_t type);
int read_inode(uint32_t n, struct dinode *p);
int write_inode(uint32_t n, struct dinode *p);
uint32_t inode_read(uint32_t n, void *buf, uint32_t sz, uint32_t off);
uint32_t inode_write(uint32_t n, void *buf, uint32_t sz, uint32_t off);

// fs.c
int fs_init(int n);
int fs_unlink(char *path);
int fs_link(char *new, char *old);
uint32_t fs_lookup(char *path, int parent);
uint32_t dir_lookup(uint32_t inum, char *name, uint32_t *poff);

// util.c
char *getname(char *path, char *name, char *parent);

// User of this fs implemention pl must provide definitions for the below two functions
// in other C files compiled along with fs.c
extern void disk_write(int n, void *buf);
extern void disk_read(int n, void *buf);
