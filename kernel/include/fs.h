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

// Each file system is in charge of a disk partition
// and the super block structure describe the whole
// file system layout. Therefore, it's important to
// realize that the file system can start almost anywhere
// in the perspective of the whole disk just like a partition
// can be formed almost anywhere in a disk.
struct superblock {
    // Start block of the file system in the disk
    uint32_t start;
    uint32_t ninodes;
    uint32_t nblock_tot;
    uint32_t nblock_log;
    uint32_t nblock_dat;
    uint32_t nblock_inode;
    // Start block of each file system section
    uint32_t slog;
    uint32_t sinode;
    uint32_t sbitmap;
    uint32_t sdata;
    // Magic number
    uint32_t magic;
};

#define NINODES_PER_BLOCK       (BLOCKSIZE/sizeof(struct dinode))
#define NDIRENTS_PER_BLOCK      (BLOCKSIZE/sizeof(struct dirent))
#define NPTRS_PER_BLOCK         (BLOCKSIZE/sizeof(uint32_t))

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
    uint16_t type;
    uint16_t major;
    uint16_t minor;
    uint16_t linkcnt;
    uint32_t size;
    uint32_t ptrs[NPTRS];
};

// Directory entry sturcture
// Each directory contains an array of directory entries,
// each pointing to an inode representing a file or another directory.
// These directory entries contain the inode number of the file they point to,
// allowing us to access the file content, as well as the associated file name.
// This structure enables file retrieval by path.
#define MAXNAME 14
struct dirent {
    uint16_t inum;
    char name[MAXNAME];
};

union block {
    struct superblock su;
    uint8_t  bytes[BLOCKSIZE];
    uint32_t ptrs[NPTRS_PER_BLOCK];
    struct dinode inodes[NINODES_PER_BLOCK];
    struct dirent dirents[NDIRENTS_PER_BLOCK];
};

// Partition table entry
struct partition {
    uint8_t  bootable;
    uint8_t  startc:8;
    uint16_t starth:10;
    uint16_t starts:6;
    uint8_t  sysid;
    uint8_t  endc:8;
    uint16_t endh:10;
    uint16_t ends:6;
    uint32_t startlba;
    uint32_t nsectors;
};

