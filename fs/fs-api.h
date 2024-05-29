typedef void (*diskfunc)(int blocknum, void *buf);
typedef int (*printfunc)(const char *fmt, ...);

int fs_init(struct partition *p, diskfunc rfunc, diskfunc wfunc, printfunc pfunc);
int fs_unlink(char *path);
int fs_link(char *new, char *old);
uint32_t fs_mknod(char *path, uint16_t type);
int fs_format(struct partition *p);
uint32_t fs_lookup(char *path);
int read_inode(uint32_t n, struct dinode *p);
int write_inode(uint32_t n, struct dinode *p);
int inode_read(uint32_t inum, void *buf, int sz, uint32_t off);
int inode_write(uint32_t inum, void *buf, int sz, uint32_t off);
