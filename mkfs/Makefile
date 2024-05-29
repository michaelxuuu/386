-include ../common.mk

INCLUDE = -I../kernel/include -I../fs/

mkfs: mkfs.c ../fs/fs.c
	gcc -DBUILD_TARGET_HOST $(INCLUDE) $^ -g -o $@

clean:
	-rm mkfs
