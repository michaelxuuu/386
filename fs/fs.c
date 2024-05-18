#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "inc.h"
#include "../kernel/fs.h"
#include "../partition/partition.h"

#define MAXPATH 64

// Encapsulating global variables within a global unnamed struct 
// not only organizes the code but also reduces naming conflicts
// with local variables. This practice prevents serious consequences
// and confusing bugs caused by compiler overrides without notification. 
// It also mitigates conflicts with global names; for example, a function
// named fs.disk_read() in another C file won't conflict with fs.fs.disk_read.
// Of course, the use of the 'static' directive can do that.
static struct {
    int init;
    struct superblock su;
    // User of this fs implemention must implement the three functions below
    // in other C files compiled along with fs.c
    diskfunc disk_read;
    diskfunc disk_write;
    printfunc printf;
} fs;

#define assert(expr) \
do {                 \
    if (!(expr))     \
        panic();     \
} while (0);

static void panic() { for (;;); }

static uint32_t bitmap_alloc() 
{
    union block b;
    fs.disk_read(fs.su.sbitmap, &b);
    for (int i = 0; i < fs.su.nblock_dat / 8; i++) {
        if (b.bytes[i] == 0xff)
            continue;
        // bytes[i] must has at least one 0 bit
        int off;
        for (off = 0; off < 8; off++)
            if (!((b.bytes[i] >> off) & 1))
                break;
        if (off + i * 8 >= fs.su.nblock_dat)
            return 0;
        assert(off != 8);
        b.bytes[i] |= 1 << off;
        fs.disk_write(fs.su.sbitmap, &b);
        return off + i * 8 + fs.su.sdata;
    }
    return 0;
}

static int bitmap_free(uint32_t n) 
{
    union block b;
    if (n < fs.su.sdata || n >= fs.su.sdata + fs.su.nblock_dat)
        return -1;
    fs.disk_read(fs.su.sbitmap, &b);
    // Double free?
    if (!(b.bytes[n/8] & (1 << (n%8))))
        return -1;
    b.bytes[n/8] &= ~(1 << (n%8));
    fs.disk_write(fs.su.sbitmap, &b);
    return 0;
}

// Caller has to make sure n is a valid inode number
int read_inode(uint32_t n, struct dinode *p) 
{
    union block b;
    if (!fs.init) {
        fs.printf("read_inode: fs not initialized\n");
        return -1;
    }
    // Read a inode block to the buffer
    fs.disk_read(fs.su.sinode + n/NINODES_PER_BLOCK, &b);
    // Read the target inode
    *p = b.inodes[n%NINODES_PER_BLOCK];
    return 0;
}

// Caller has to make sure n is a valid inode number
int write_inode(uint32_t n, struct dinode *p) 
{
    union block b;
    if (!fs.init) {
        fs.printf("write_inode: fs not initialized\n");
        return -1;
    }
    fs.disk_read(fs.su.sinode + n/NINODES_PER_BLOCK, &b);
    b.inodes[n%NINODES_PER_BLOCK] = *p;
    fs.disk_write(fs.su.sinode + n/NINODES_PER_BLOCK, &b);
    return 0;
}

// Given a index into the 'ptrs' array in an inode,
// this function returns the indirection level of the
// pointer entry - if it's a direct, singly-indirect,
// or a doubly-indirect pointer.
static int get_ilevel(int ptr_idx) 
{
    if (ptr_idx < NDIRECT)
        return 0;
    else if (ptr_idx < NDIRECT+NINDRECT)
        return 1;
    else
        return 2;
}

// There are three types of blocks:
//
// 1. Data blocks
// 2. Singly-indirect blocks
// 3. Doubly-indirect blocks
//
// We consider all these blocks to be *indirect* blocks
// and distinguish them by their "indirect level," ilevel for short.
//
// Below are the ilevels for each type of block:
// Data blocks:             ilevel=0
// Singly-indirect blocks:  ilevel=1
// Doubly-indirect blocks:  ilevel=2

// Frees a general indirect block (can be a data block if ilevel=0). Recursively
// frees all sub-level blocks based on the ilevel value. For example, an initial
// call with ilevel=2 for a doubly-indirect block will recursively free all
// singly-indirect blocks and their respective data blocks.
static int free_indirect(uint32_t n, int ilevel) 
{
    // ilevel=0 is the base case: it is a data block
    if (!ilevel) {
        assert(bitmap_free(n));
        return 0;
    }
    // Not a data block. Then it must be an indirect block.
    // We treat doubly-indirect and singly-indirect blocks
    // the same since they are all just a block of pointers.
    union block b;
    // Read the indirect block
    fs.disk_read(n, &b);
    // Free the block after reading into memory
    assert(!bitmap_free(n));
    // Recursively free all referenced sub-level blocks
    for (int i = 0; i < NPTRS_PER_BLOCK; i++)
        if (b.ptrs[i])
            free_indirect(b.ptrs[i], ilevel - 1); // Decrement ilevel per recursion
    return 0;
}

// Free an inode. Also need to free all referenced data blocks.
int free_inode(uint32_t n) 
{
    union block b;
    struct dinode di;
    if (!fs.init) {
        fs.printf("free_inode: fs not initialized\n");
        return -1;
    }
    if (n >= fs.su.ninodes) {
        fs.printf("free_inode: inode number %d is out of range", n);
        return -1;
    }
    read_inode(n, &di);
    di.type = 0;
    for (int i = 0; i < NPTRS; i++)
        if (di.ptrs[i])
            free_indirect(di.ptrs[i], get_ilevel(i));
    write_inode(n, &di);
    return 0;
}

uint32_t alloc_inode(uint16_t type) 
{
    // Invalid inode type, return error
    if (type > T_DEV) {
        fs.printf("alloc_inode: invalid inode type\n");
        return NULLINUM;
    }
    // Loop through all blocks for inode
    for (int i = 0; i < fs.su.nblock_inode; i++) {
        union block b;
        // Read current inode block to the buffer
        fs.disk_read(i + fs.su.sinode, &b);
        for (int j = 0; j < NINODES_PER_BLOCK; j++) {
            // Found a unallocated inode
            if (!b.inodes[j].type) {
                struct dinode *p = &b.inodes[j];
                memset(p, 0, sizeof(*p));
                p->type = type;
                // Write back updated inode block
                fs.disk_write(i + fs.su.sinode, &b);
                return i * NINODES_PER_BLOCK + j;
            }
        }
    }
    return NULLINUM;
}

// This is the last argument of recursive_rw(), and the struct 
// members are shared among all recursive calls simultaneously.
// This reduces the number of arguments to be passed and makes 
// the code more concise and readable. The struct contains elements
// that have only one global copy across all instances of recursive_rw(),
// generated from a single call to inode_rw(), while each recursive_rw() 
// call has their own private copies of the other two arguments.
struct share_arg {
    uint32_t boff;   // Current block offset within a file
    uint32_t sblock; // Start block
    uint32_t eblock; // End block
    // Note: For each block pointed to by pp, we test if the data block it covers
    // or itself if it is a data block (*boff + nblocks linked to it) overlaps with 
    // the [sblock, eblock] interval, and we skip this block if not.
    uint32_t off;    // Same off in inode_rw()
    char *buf;  // Same buf in inode_rw()
    uint32_t left;   // Number of bytes left
    int w;      // Recursive write? Recursive read if 0
};

static int recursive_rw(
    uint32_t *pp,    // Pointer to a block pointer (which could be in an inode or an indirect pointer that caller traverses)
    uint32_t ilevel, // Recursion level. 0 means we've reached a data block.
    struct share_arg *sa
) {
    // Do we skip this indirect block?
    // Compute data *block coverage* of this indirect (or data) block: [sblock, eblock)
    uint32_t sblock = sa->boff;
    uint32_t eblock = sa->boff;
    if (ilevel == 0)
        eblock += 1;
    if (ilevel == 1)
        eblock += NPTRS_PER_BLOCK;
    if (ilevel == 2)
        eblock += NPTRS_PER_BLOCK*NPTRS_PER_BLOCK;
    // Do [sblock, eblock) and [sa->sblock, sa->eblock], the *block coverage* of this w/r operation overlap?
    if (!(sblock <= sa->eblock && sa->sblock < eblock)) {
        sa->boff = eblock;
        sa->off += (eblock - sblock) * BLOCKSIZE;
        return 0;
    }
    if (sa->w) {
        // This indirect (or data) block is involved in this w/r,
        // so it should not be null and we should allocate it if null.
        int zero = 0;
        if (!*pp && ilevel)
            zero = 1;
        if (!*pp && !(*pp = bitmap_alloc()))
            return -1; // ran out of free blocks
        if (zero) {
            char zeros[BLOCKSIZE] = {0};
            fs.disk_write(*pp, &zeros);
        }
    } else {
        // Handle reading sparse files
        if (!*pp) {
            uint32_t sz = (eblock - sblock) * BLOCKSIZE;
            if (sa->left < sz)
                sz = sa->left;
            memset(sa->buf, 0, sz);
            sa->buf += sz;
            sa->left -= sz;
            sa->off += sz;
            sa->boff = eblock;
            return 0;
        }
    }
    union block b;
    // It is an indirect block, start recursion.
    if (ilevel) {
        fs.disk_read(*pp, &b);
        for (int i = 0; i < NPTRS_PER_BLOCK; i++)
            if (recursive_rw(&b.ptrs[i], ilevel - 1, sa)) {
                // If a write failed half way, we do *not* roll back, but
                // leave the blocks already written and abort. However,
                // we DO need to update the indirect block that has been
                // modified. That's why we're writing back to disk this
                // indirect block.
                if (sa->w) fs.disk_write(*pp, &b);
                return -1;
            }
        fs.disk_write(*pp, &b);
        return 0;
    }
    // It's a data block.
    uint32_t start = sa->off % BLOCKSIZE;
    uint32_t sz = sa->left < (BLOCKSIZE - start) ? sa->left : (BLOCKSIZE - start);
    fs.disk_read(*pp, &b);
    if (sa->w) {
        memcpy(&b.bytes[start], sa->buf, sz);
        fs.disk_write(*pp, &b);
    } else
        memcpy(sa->buf, &b.bytes[start], sz);
    sa->buf += sz;
    sa->left -= sz;
    sa->off += sz;
    sa->boff = eblock; // Must update boff.
    return 0;
}

static uint32_t inode_rw(uint32_t n, void *buf, uint32_t sz, uint32_t off, int w)
{
    struct dinode di;
    uint32_t sbyte = off;
    uint32_t ebyte = off + sz;
    // Invalid inode number
    if (n >= fs.su.ninodes) {
        fs.printf("inode number %d is out of range", n);
        return -1;
    }
    // Read inode structure
    read_inode(n, &di);
    if (!w && sbyte >= di.size)
        return 0;
    if (!w && ebyte >= di.size) {
        ebyte = di.size;
        sz = di.size - sbyte;
    }
    uint32_t sblock = sbyte/BLOCKSIZE;
    uint32_t eblock = ebyte/BLOCKSIZE;
    struct share_arg *sa = &(struct share_arg){
        .boff = 0,
        .sblock = sblock,
        .eblock = eblock,
        .off = off,
        .buf = buf,
        .left = sz,
        .w = w
    };

    for (int i = 0; i < NPTRS; i++)
        if (recursive_rw(&di.ptrs[i], get_ilevel(i), sa))
            break;
    uint32_t consumed = sz - sa->left;
    ebyte = off + consumed; // ebyte should remain unchanged if consumed equals sz, 
                            // indicating that the required amount of bytes has been successfully 
                            // consumed from buf (write) or from disk (read)
    di.size = di.size > ebyte ? di.size : ebyte; // update inode size in case it's a write operation
    if (w)
        assert(!write_inode(n, &di)); // update inode
    return consumed;
}

uint32_t inode_write(uint32_t n, void *buf, uint32_t sz, uint32_t off) {
    if (!fs.init) {
        fs.printf("inode_write: fs not initialized\n");
        return -1;
    }
    return inode_rw(n, buf, sz, off, 1);
}

uint32_t inode_read(uint32_t n, void *buf, uint32_t sz, uint32_t off) {
    if (!fs.init) {
        fs.printf("inode_read: fs not initialized\n");
        return -1;
    }
    return inode_rw(n, buf, sz, off, 0);
}

// Look up 'name' under the directory pointed to by 'inum.'
// Return the inum of the dirent containing 'name' if found and NULLINUM (0) otherwise.
// Write the offset of the dirent found into *poff if it's not NULL.
static uint32_t dir_lookup(uint32_t inum, char *name, uint32_t *poff)
{
    struct dinode di;
    read_inode(inum, &di);
    // Not a directory
    if (di.type != T_DIR)
        return NULLINUM;
    uint32_t off = 0;
    for (int i = 0; i < di.size / sizeof(struct dirent); 
        i++, off += sizeof(struct dirent)) {
        struct dirent de;
        inode_read(inum, &de, sizeof(struct dirent), off);
        if (!strcmp(name, de.name)) {
            if (poff)
                *poff = off;
            return de.inum;
        }
    }
    return NULLINUM;
}

// Find the inode corresponds to the given path.
// If parent = 1, stop one level early at the parent dir.
uint32_t fs_lookup(char *path, int parent) {
    if (!path)
        return NULLINUM;
    int l = strnlen(path, MAXPATH);
    if (l > MAXPATH - 1)
        return NULLINUM;
    // This function only start finding from root
    // restricting path must be preceeded by '/'
    if (path[0] != '/')
        return NULLINUM;
    uint32_t inum = ROOTINUM; // start from root
    path += 1; // skip the leading slash
    for (;;) {
        if (!*path)
            break;
        // copy the next name from path
        char name[MAXNAME];
        for (l = 0; *path && *path != '/'; l++, path++) {
            if (l >= MAXNAME)
                return NULLINUM;
            name[l] = *path;
        }
        // null-term the name
        name[l] = 0;
        // skip following slashes
        for (; *path && *path == '/'; path++);
        // stop one level early to return the inode of the parent dir
        if (!*path && parent)
            break;
        if (!(inum = dir_lookup(inum, name, 0)))
            return NULLINUM;
    }
    return inum;
}

// Each path can be seen as parent/name
// * Copy the "name" part into the 'name' buf
// * Optionally copy the "parent" part into the 'parent' buf
// * Return a pointer to the start of the "name" part on success
// * Return 0 when no specified, as in path "////////" or ""
static char *getname(char *path, char *name, char *parent) {
    if (!path)
        return 0;
    int len = strlen(path);
    if (!len)
        return 0;
    char *p = path+len-1;
    for (; p!=path && p[0]=='/'; p--);
    if (p==path && len!=1)
        return 0;
    char *pp = p;
    for (; p!=path && p[0]!='/'; p--);
    if (p[0]=='/')
        p++;
    if (pp-p+1>MAXNAME-1)
        return 0;
    strncpy(name, p, pp-p+1);
    name[pp-p+1]=0;
    if (parent) {
        strncpy(parent, path, p-path);
        parent[p-path]=0;
    }
    return p;
}

// path = parent(dir)/name
// Create an inode and link it under "parent" with "name"
int fs_mknod(char *path, uint16_t type) {
    uint32_t n;
    char parent[MAXPATH];
    char name[MAXNAME];
    struct dinode di;
    struct dirent de;
    if (!getname(path, name, parent)) {
        fs.printf("no name specified\n");
        return -1;
    }
    // Parent path must points to a valid *directory* inode.
    n = fs_lookup(path, 1);
    if (n == NULLINUM) {
        fs.printf("parent directory not found %s\n", parent);
        return -1;
    }
    read_inode(n, &di);
    if (di.type != T_DIR) {
        fs.printf("not a directory %s\n", parent);
        return -1;
    }
    // Check for duplicates
    if (dir_lookup(n, name, 0)) {
        fs.printf("%s found under %s\n", name, parent);
        return -1;
    }
    // Create an inode.
    de.inum = alloc_inode(type);
    if (de.inum == NULLINUM) {
        fs.printf("failed to allocate inode\n");
        return -1;
    }
    // Link it to the "parent" dir.
    strncpy(de.name, name, MAXNAME);
    if (inode_write(n, &de, sizeof de, di.size) != sizeof de) {
        free_inode(de.inum);
        fs.printf("failed to write %s\n", parent);
        return -1;
    }
    // Inc link count to 1 cus now "parent" dir points to it.
    read_inode(de.inum, &di);
    di.linkcnt++;
    write_inode(de.inum, &di);
    return 0;
}

// path=parent/name
// Remove the directory entry from "parent," and decrement
// the link count of the corresponding inode and free it
// if the link count reaches to 0.
int fs_unlink(char *path) {
    uint32_t n;
    uint32_t nn;
    uint32_t off;
    struct dinode di;
    struct dirent de;
    char name[MAXNAME];
    char parent[MAXPATH];
    if (!getname(path, name, parent)) {
        fs.printf("no name specified\n");
        return -1;
    }
    // Parent path must points to a valid *directory* inode.
    n = fs_lookup(path, 1);
    if (n == NULLINUM) {
        fs.printf("parent directory not found %s\n", parent);
        return -1;
    }
    read_inode(n, &di);
    if (di.type != T_DIR) {
        fs.printf("not a directory %s\n", parent);
        return -1;
    }
    // "name" must be in the parent directory
    nn = dir_lookup(n, name, &off);
    if ((nn = dir_lookup(n, name, &off)) ==  NULLINUM) {
        fs.printf("%s not found under %s\n", name, parent);
        return -1;
    }
    // Zero the directory entry found
    memset(&de, 0, sizeof de);
    assert(inode_write(n, &de, sizeof de, off) == sizeof de);
    // Decrement the link count cus "name" no longer points to that inode
    read_inode(nn, &di);
    di.linkcnt--;
    if (!di.linkcnt) {
        // Free the inode if link count reaches 0
        assert(free_inode(nn));
        return 0;
    }
    write_inode(nn, &di);
    return 0;
}

// Create a "new" path that points to the same inode the "old" path points to:
// Given old_parent/dirent{old_name, inum},
// create new_parent/dirent{new_name, inum}
int fs_link(char *new, char *old) {
    uint32_t n;
    uint32_t nn;
    uint32_t off;
    struct dinode di;
    struct dirent de;
    char name[MAXNAME];
    char parent[MAXPATH];
    if (!getname(new, name, parent)) {
        fs.printf("no name specified\n");
        return -1;
    }
    // The "old" path must point a valid inode.
    if ((nn = fs_lookup(old, 0)) != NULLINUM) {
        fs.printf("no such file or directory %s\n", old);
        return -1;
    }
    // Parent path must point to a *directory* inode.
    if ((n = fs_lookup(new, 1)) == NULLINUM) {
        fs.printf("parent directory not found %s\n", parent);
        return -1;
    }
    read_inode(n, &di);
    if (di.type != T_DIR) {
        fs.printf("not a directory %s\n", parent);
        return -1;
    }
    // "name" must *not* be in "parent" already.
    if (dir_lookup(n, name, 0) != NULLINUM) {
        fs.printf("%s found under %s\n", name, parent);
        return -1;
    }
    // Create a new directory entry with "name"
    // the same inode number as "old"
    de.inum = nn;
    strncpy(de.name, name, MAXNAME);
    if (inode_write(n, &de, sizeof de, off) != sizeof de) {
        fs.printf("failed to append to dir\n");
        return -1;
    }
    // Increment the link count as now new also points to it.
    read_inode(nn, &di);
    di.linkcnt++;
    write_inode(nn, &di);
    return 0;
}

int fs_init(struct partition *p, diskfunc rfunc, diskfunc wfunc, printfunc pfunc) {
    if (fs.init || !rfunc || !wfunc || !pfunc)
        return -1;
    fs.disk_read = rfunc;
    fs.disk_write = wfunc;
    fs.printf = pfunc;
    union block b;
    fs.disk_read(p->startlba, &b);
    if (b.su.magic != FSMAGIC) {
        fs.su = b.su;
        return -1;
    }
    fs.su = b.su;
    fs.init = 1;
    return 0;
}

int fs_format(struct partition *p) {
    union block b = { .bytes = {0} } ;
    // Zero the partition
    for (int i = 0; i < fs.su.nblock_tot; i++)
        fs.disk_write(fs.su.start + i, &b);
    // Prep the super block
    b.su.start = p->startlba;
    b.su.ninodes = NINODES;
    b.su.nblock_tot = p->nsectors;
    b.su.nblock_log = NBLOCKS_LOG;
    b.su.nblock_inode = NINODES / NINODES_PER_BLOCK;
    b.su.nblock_dat = b.su.nblock_tot - (NBLOCKS_LOG + b.su.nblock_inode + 1 + 1);
    b.su.slog = p->startlba + 1;
    b.su.sinode = b.su.slog + NBLOCKS_LOG;
    b.su.sbitmap = b.su.sinode + b.su.nblock_inode;
    b.su.sdata = b.su.sbitmap + 1;
    b.su.magic = FSMAGIC;
    // Write the super block to the disk
    fs.disk_write(p->startlba, &b);
    // Reserve inode 0 and 1 (0 for NULL and 1 for the root directory)
    fs.su = b.su;
    alloc_inode(T_DIR);
    alloc_inode(T_DIR);
    return 0;
}
