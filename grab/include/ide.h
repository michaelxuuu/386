void ide_init();
int ide_sel(int drivenum);
struct partition *ide_get_partitions();
void ide_write_lba(int lba, void *buf);
void ide_read_lba(int lba, void *buf);
