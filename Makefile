.PHONY: gdb gdb-in-new-bash-session \
		qemu kill-qemu \
		grab mkfs \
		mkvhd mkvhd.1 mkvhd.2 mkvhd.3 \
		clean

PATH_MKFS = ./mkfs
PATH_GRAB = ./grab
PATH_FS = ./grab
CONTAINER = fdisk

gdb: qemu-gdb gdb-in-new-bash-session kill-qemu

gdb-in-new-bash-session:
	echo 'x86_64-elf-gdb -ix .gdb/gdbinit && exit' > init.tmp
	bash --init-file init.tmp

qemu-gdb: mkvhd
	qemu-system-i386 \
		-drive file=vhd,if=ide,index=0,media=disk,format=raw \
		-drive file=vhd1,if=ide,index=1,media=disk,format=raw \
		-drive file=vhd2,if=ide,index=2,media=disk,format=raw \
		-drive file=vhd3,if=ide,index=3,media=disk,format=raw \
		-s -S &

kill-qemu:
	pkill -9 qemu

mkvhd: mkvhd.1 mkvhd.2 mkvhd.3

mkvhd.1: vhd
vhd:
# create vhd
	dd if=/dev/zero of=vhd bs=512 count=1024
	dd if=/dev/zero of=vhd1 bs=512 count=512
	dd if=/dev/zero of=vhd2 bs=512 count=256
	dd if=/dev/zero of=vhd3 bs=512 count=128
# partition vhd
	docker start $(CONTAINER)
	docker exec $(CONTAINER) bash -c "printf \"n\np\n1\n65\n1023\na\nw\n\" | fdisk /host/$(shell pwd)/vhd"
	docker exec $(CONTAINER) bash -c "printf \"n\np\n1\n\n\n\nw\n\" | fdisk /host/$(shell pwd)/vhd1"
	docker exec $(CONTAINER) bash -c "printf \"n\np\n1\n\n\n\nw\n\" | fdisk /host/$(shell pwd)/vhd2"
	docker exec $(CONTAINER) bash -c "printf \"n\np\n1\n\n\n\nw\n\" | fdisk /host/$(shell pwd)/vhd3"
# list partitions
	docker exec $(CONTAINER) bash -c "fdisk -l /host/$(shell pwd)/vhd"

mkvhd.2: $(PATH_MKFS)/*.c $(PATH_FS)/*.c | mkfs
# create a file system on vhd
	echo "mkdir /boot\n mkdir /home\n quit" | $(PATH_MKFS)/mkfs vhd 1
	touch $@

mkvhd.3: $(PATH_GRAB)/*.c | grab
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
	-rm vhd *.tmp mkvhd.* vhd*
