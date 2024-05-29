void ide_init();
void ide_list();
void ide_sel(int drivenum);
struct partition *ide_get_partitions(int num);
void ide_write_lba(int lba, void *buf);
void ide_read_lba(int lba, void *buf);
