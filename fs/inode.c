#include "inc.h"

static uint32_t bitmap_alloc() 
{
    union block b;
    disk_read(su.sbitmap, &b);
    for (int i = 0; i < su.nblock_dat / 8; i++) {
        if (b.bytes[i] == 0xff)
            continue;
        // bytes[i] must has at least one 0 bit
        int off;
        for (off = 0; off < 8; off++)
            if (!((b.bytes[i] >> off) & 1))
                break;
        if (off + i * 8 >= su.nblock_dat)
            return 0;
        assert(off != 8);
        b.bytes[i] |= 1 << off;
        disk_write(su.sbitmap, &b);
        return off + i * 8 + su.sdata;
    }
    return 0;
}


static int bitmap_free(uint32_t n) 
{
    union block b;
    if (n < su.sdata || n >= su.sdata + su.nblock_dat)
        return -1;
    disk_read(su.sbitmap, &b);
    // Double free?
    if (!(b.bytes[n/8] & (1 << (n%8))))
        return -1;
    b.bytes[n/8] &= ~(1 << (n%8));
    disk_write(su.sbitmap, &b);
    return 0;
}


int read_inode(uint32_t n, struct dinode *p) 
{
    union block b;
    // Read a inode block to the buffer
    disk_read(su.sinode + n/NINODES_PER_BLOCK, &b);
    // Read the target inode
    *p = b.inodes[n%NINODES_PER_BLOCK];
    return 0;
}

int write_inode(uint32_t n, struct dinode *p) 
{
    union block b;
    disk_read(su.sinode + n/NINODES_PER_BLOCK, &b);
    b.inodes[n%NINODES_PER_BLOCK] = *p;
    disk_write(su.sinode + n/NINODES_PER_BLOCK, &b);
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
    disk_read(n, &b);
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
    if (n >= su.ninodes)
        return -1;
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
    if (type > T_DEV)
        return -1;
    // Loop through all blocks for inode
    for (int i = 0; i < su.nblock_inode; i++) {
        union block b;
        // Read current inode block to the buffer
        disk_read(i + su.sinode, &b);
        for (int j = 0; j < NINODES_PER_BLOCK; j++) {
            // Found a unallocated inode
            if (!b.inodes[j].type) {
                struct dinode *p = &b.inodes[j];
                memset(p, 0, sizeof(*p));
                p->type = type;
                // Write back updated inode block
                disk_write(i + su.sinode, &b);
                return i * NINODES_PER_BLOCK + j;
            }
        }
    }
    return -1;
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
            disk_write(*pp, &zeros);
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
        disk_read(*pp, &b);
        for (int i = 0; i < NPTRS_PER_BLOCK; i++)
            if (recursive_rw(&b.ptrs[i], ilevel - 1, sa)) {
                // If a write failed half way, we do *not* roll back, but
                // leave the blocks already written and abort. However,
                // we DO need to update the indirect block that has been
                // modified. That's why we're writing back to disk this
                // indirect block.
                if (sa->w) disk_write(*pp, &b);
                return -1;
            }
        disk_write(*pp, &b);
        return 0;
    }
    // It's a data block.
    uint32_t start = sa->off % BLOCKSIZE;
    uint32_t sz = sa->left < (BLOCKSIZE - start) ? sa->left : (BLOCKSIZE - start);
    disk_read(*pp, &b);
    if (sa->w) {
        memcpy(&b.bytes[start], sa->buf, sz);
        disk_write(*pp, &b);
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
    if (n >= su.ninodes)
        return -1;
    // Read inode structure
    if (read_inode(n, &di))
        return -1;
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
    return inode_rw(n, buf, sz, off, 1);
}

uint32_t inode_read(uint32_t n, void *buf, uint32_t sz, uint32_t off) {
    return inode_rw(n, buf, sz, off, 0);
}