-include ../include.mk

mkfs: mkfs.c $(PATH_FS)/fs.c
	gcc -DBUILD_TARGET_HOST $^ -g -o $@

clean:
	-rm mkfs
