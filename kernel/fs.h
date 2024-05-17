#ifndef _fs_h_
#define _fs_h_

#include "types.h"

// FS layout
//
// super block | log blocks | inode blocks | bitmap block | data blocks

// Fixed fs parameters
#define BLOCKSIZE 512
#define NBLOCKS_LOG 30
#define NINODES 200
#define FSMAGIC 0xdeadbeef
#define NULLINUM 0
#define ROOTINUM 1 // root directory inode number

struct superblock {
    u32 ninodes;
    u32 nblock_tot;
    u32 nblock_log;
    u32 nblock_dat;
    u32 nblock_inode;
    // Start block of each disk section
    u32 ssuper;
    u32 slog;
    u32 sinode;
    u32 sbitmap;
    u32 sdata;
    u32 magic;
};

#define NINODES_PER_BLOCK       (BLOCKSIZE/sizeof(struct dinode))
#define NDIRENTS_PER_BLOCK      (BLOCKSIZE/sizeof(struct dirent))
#define NPTRS_PER_BLOCK         (BLOCKSIZE/sizeof(u32))

// #direct, indirect, and doubly-indirect, and total pointers in an inode
#define NDIRECT                 10
#define NINDRECT                2
#define NDINDRECT               1
#define NPTRS                   (NDIRECT+NINDRECT+NDINDRECT)

#define T_REG 1
#define T_DIR 2
#define T_DEV 3
// On-disk inode sturcture
struct dinode {
    u16 type;
    u16 major;
    u16 minor;
    u16 linkcnt;
    u32 size;
    u32 ptrs[NPTRS];
};

// Directory entry sturcture
// Each directory contains an array of directory entries,
// each pointing to an inode representing a file or another directory.
// These directory entries contain the inode number of the file they point to,
// allowing us to access the file content, as well as the associated file name.
// This structure enables file retrieval by path.
#define MAXNAME 14
struct dirent {
    u16 inum;
    char name[MAXNAME];
};

union block {
    struct superblock su;
    u8  bytes[BLOCKSIZE];
    u32 ptrs[NPTRS_PER_BLOCK];
    struct dinode inodes[NINODES_PER_BLOCK];
    struct dirent dirents[NDIRENTS_PER_BLOCK];
};

#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002

#endif
