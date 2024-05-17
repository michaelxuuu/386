#include "inc.h"

struct superblock su;

// Look up 'name' under the directory pointed to by 'inum.'
// Return the inum of the dirent containing 'name' if found and NULLINUM (0) otherwise.
// Write the offset of the dirent found into *poff if it's not NULL.
u32 dir_lookup(u32 inum, char *name, u32 *poff)
{
    struct dinode di;
    read_inode(inum, &di);
    // Not a directory
    if (di.type != T_DIR)
        return NULLINUM;
    u32 off = 0;
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
u32 fs_lookup(char *path, int parent) {
    if (!path)
        return NULLINUM;
    int l = strnlen(path, MAXPATH);
    if (l > MAXPATH - 1)
        return NULLINUM;
    // This function only start finding from root
    // restricting path must be preceeded by '/'
    if (path[0] != '/')
        return NULLINUM;
    u32 inum = ROOTINUM; // start from root
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

#include <stdio.h>
// Install fs onto the specified partition
// n: partition number
int fs_init(int n) {
    union block b;
    n--; // Convert to 0-based indexing
    disk_read(0, &b); // Read the first sector
    if (!ismbr(&b)) // Check if the first sector is the mbr from whom we need the partition table
        return -1;
    // Obtain partition table from the mbr read
    struct par partble[4];
    struct par *p = (struct par *)(&b.bytes[512] - 2 - sizeof(struct par) * 4);
    for (int i = 0; i < 4; partble[i++] = *p++);
    if (!partble[n].sysid) // Is the specified partition used?
        return -1;
    // Read the block where the superblock is supposedly at
    int super_num = partble[n].startlba;
    disk_read(super_num, &b);
    // Check if the magic number exists - aka. if there is already an fs
    if (b.su.magic == FSMAGIC) {
        // Save a copy of the super block in the memory
        su = b.su;
        return 0;
    }
// Install fs in the specified partition
    // Zero the partition
    char buf[BLOCKSIZE] = {0};
    for (int i = 0; i < partble[n].nsectors; i++)
        disk_write(partble[n].startlba + i, buf);
    // Prep super block
    b.su.ninodes = NINODES;
    b.su.nblock_tot = partble[n].nsectors;
    b.su.nblock_log = NBLOCKS_LOG;
    b.su.nblock_inode = NINODES / NINODES_PER_BLOCK;
    b.su.nblock_dat = b.su.nblock_tot - (NBLOCKS_LOG + b.su.nblock_inode + 1 + 1);
    b.su.ssuper = super_num;
    b.su.slog = super_num + 1;
    b.su.sinode = b.su.slog + NBLOCKS_LOG;
    b.su.sbitmap = b.su.sinode + b.su.nblock_inode;
    b.su.sdata = b.su.sbitmap + 1;
    b.su.magic = FSMAGIC;
    disk_write(b.su.ssuper, &b); // Write super block to disk
    su = b.su; // Save a copy of the super block in the memory
    // Reserve inode 0 and 1 (0 for NULL and 1 for the root directory)
    alloc_inode(T_DIR);
    alloc_inode(T_DIR);
    return 0;
}


// path = parent(dir)/name
// Create an inode and link it under "parent" with "name"
int fs_mknode(char *path, u16 type) {
    u32 n;
    char parent[MAXPATH];
    char name[MAXNAME];
    struct dinode di;
    struct dirent de;
    if (!getname(path, name, parent)) {
        return -1;
    }
    // Parent path must points to a valid *directory* inode.
    n = fs_lookup(path, 1);
    if (n == NULLINUM) {
        return -1;
    }
    read_inode(n, &di);
    if (di.type != T_DIR) {
        return -1;
    }
    // Check for duplicates
    if (dir_lookup(n, name, 0)) {
        return -1;
    }
    // Create an inode.
    de.inum = alloc_inode(type);
    if (de.inum == NULLINUM) {
        return -1;
    }
    // Link it to the "parent" dir.
    strncpy(de.name, name, MAXNAME);
    if (inode_write(n, &de, sizeof de, di.size) != sizeof de) {
        free_inode(de.inum);
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
    u32 n;
    u32 nn;
    u32 off;
    struct dinode di;
    struct dirent de;
    char name[MAXNAME];
    char parent[MAXPATH];
    if (!getname(path, name, parent)) {
        return -1;
    }
    // Parent path must points to a valid *directory* inode.
    n = fs_lookup(path, 1);
    if (n == NULLINUM) {
        return -1;
    }
    read_inode(n, &di);
    if (di.type != T_DIR) {
        return -1;
    }
    // "name" must be in the parent directory
    nn = dir_lookup(n, name, &off);
    if ((nn = dir_lookup(n, name, &off)) ==  NULLINUM) {
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
    u32 n;
    u32 nn;
    u32 off;
    struct dinode di;
    struct dirent de;
    char name[MAXNAME];
    char parent[MAXPATH];
    if (!getname(new, name, parent)) {
        return -1;
    }
    // The "old" path must point a valid inode.
    if ((nn = fs_lookup(old, 0)) != NULLINUM) {
        return -1;
    }
    // Parent path must point to a *directory* inode.
    if ((n = fs_lookup(new, 1)) == NULLINUM) {
        return -1;
    }
    read_inode(n, &di);
    if (di.type != T_DIR) {
        return -1;
    }
    // "name" must *not* be in "parent" already.
    if (dir_lookup(n, name, 0) != NULLINUM) {
        return -1;
    }
    // Create a new directory entry with "name"
    // the same inode number as "old"
    de.inum = nn;
    strncpy(de.name, name, MAXNAME);
    if (inode_write(n, &de, sizeof de, off) != sizeof de) {
        return -1;
    }
    // Increment the link count as now new also points to it.
    read_inode(nn, &di);
    di.linkcnt++;
    write_inode(nn, &di);
    return 0;
}
