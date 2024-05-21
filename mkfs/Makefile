-include ../common.mk

mkfs: mkfs.c $(PATH_FS)/fs.c
	gcc -DBUILD_TARGET_HOST $(INCLUDE) $^ -g -o $@

clean:
	-rm mkfs
