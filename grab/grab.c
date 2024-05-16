#include <stdio.h>
#include <stdlib.h>

struct partition_table_entry {
    unsigned char stat;
    unsigned char start_chs[3];
    unsigned char type;
    unsigned char end_chs[3];
    unsigned int start_lba;
    unsigned int sector_cnt;
};

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "usage: grab <vhd_name> <root_partition_num>\n");
        exit(1);
    }

}
