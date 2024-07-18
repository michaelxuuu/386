typedef void (*diskfunc)(int blocknum, void *buf);
typedef int (*printfunc)(const char *fmt, ...);

int fs_init(struct partition *p, diskfunc rfunc, diskfunc wfunc,
            printfunc pfunc);
int fs_format(struct partition *p);
uint32_t fs_mknod(char *path, uint16_t type);
uint32_t fs_lookup(char *path);
int fs_geti(uint32_t inum, struct dinode *di);
int fs_read(uint32_t inum, void *buf, int sz, uint32_t off);
int fs_write(uint32_t inum, void *buf, int sz, uint32_t off);
