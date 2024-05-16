.PHONY: gdb _gdb qemu clean kill clean grab

GRAB_PATH=./grab

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
	make -C $(GRAB_PATH) clean

vhd: grab
	dd if=/dev/urandom of=vhd bs=1K count=32
	dd bs=512 if=$(GRAB_PATH)/boot.bin of=$@ count=1 conv=notrunc
	dd bs=512 if=$(GRAB_PATH)/test.bin of=$@ seek=1 count=63 conv=notrunc

grab:
	make -C $(GRAB_PATH)
