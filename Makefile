.PHONY: gdb _gdb qemu clean kill clean bootloader

PATH_BOOTLOADER=./bootloader

gdb: qemu _gdb kill

_gdb:
	echo 'x86_64-elf-gdb -ix .gdb/gdbinit && exit' > init.tmp
	bash --init-file init.tmp

qemu: vhd
	qemu-system-i386 vhd -s -S &

kill:
	pkill -9 qemu

clean:
	rm -rf vhd *.tmp
	make -C $(PATH_BOOTLOADER) clean

vhd: bootloader
	dd if=/dev/urandom of=vhd bs=1K count=32
	dd bs=512 if=$(PATH_BOOTLOADER)/boot.bin of=$@ count=1 conv=notrunc
	dd bs=512 if=$(PATH_BOOTLOADER)/test.bin of=$@ seek=1 count=63 conv=notrunc

bootloader:
	make -C $(PATH_BOOTLOADER)
