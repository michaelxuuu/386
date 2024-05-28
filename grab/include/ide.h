void ide_init();
void ide_list();
void ide_sel(int drivenum);
struct partition *ide_get_partitions(int num);
void ide_write(int n, void *buf);
void ide_read(int n, void *buf);
