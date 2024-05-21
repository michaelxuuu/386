.PHONY: gdb gdb-in-new-bash-session \
		qemu kill-qemu \
		grab mkfs \
		clean

PATH_MKFS = ./mkfs
PATH_GRAB = ./grab
CONTAINER = fdisk

gdb: qemu gdb-in-new-bash-session kill-qemu

gdb-in-new-bash-session:
	echo 'x86_64-elf-gdb -ix .gdb/gdbinit && exit' > init.tmp
	bash --init-file init.tmp

qemu: mkvhd
	qemu-system-i386 vhd -s -S &

kill-qemu:
	pkill -9 qemu

mkvhd: vhd.1 vhd.2 vhd.3

vhd.1: vhd
vhd:
# create vhd
	dd if=/dev/zero of=vhd bs=512 count=1024
# partition vhd
	docker start $(CONTAINER)
	docker exec $(CONTAINER) bash -c "printf \"n\np\n1\n65\n1023\na\nw\n\" | fdisk /host/$(shell pwd)/vhd"
# list partitions
	docker exec $(CONTAINER) bash -c "fdisk -l /host/$(shell pwd)/vhd"

vhd.2: $(PATH_MKFS)/*.c | mkfs
# create a file system on vhd
	echo "quit" | $(PATH_MKFS)/mkfs vhd 1
	touch $@

vhd.3: $(PATH_GRAB)/*.c | grab
# install the grab bootloader on vhd
	dd if=$(PATH_GRAB)/stage1.bin of=vhd bs=1 count=$(shell echo $$((512-2-16*4))) conv=notrunc
	dd if=$(PATH_GRAB)/stage2.bin of=vhd bs=512 count=63 seek=1 conv=notrunc
	touch $@

docker:
	docker run -d -it -v $(shell pwd):/host/$(shell pwd) --name $(CONTAINER) --platform linux/amd64 ubuntu:latest
	docker exec $(CONTAINER) bash -c "apt update; apt -y install fdisk"

grab:
	make -C $(PATH_GRAB)

mkfs:
	make -C $(PATH_MKFS)

clean:
	make -C $(PATH_GRAB) clean
	make -C $(PATH_MKFS) clean
	-rm vhd *.tmp vhd.*
