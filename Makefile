.PHONY: gdb gdb-in-new-bash-session \
		qemu kill-qemu \
		grab mkfs \
		vhd vhd.1 vhd.2 vhd.3 vhd.4 vhd.5 \
		clean

GRAB=./grab/
MKFS=./mkfs/
CONTAINER=fdisk

gdb: qemu gdb-in-new-bash-session kill-qemu

gdb-in-new-bash-session:
	echo 'x86_64-elf-gdb -ix .gdb/gdbinit && exit' > init.tmp
	bash --init-file init.tmp

qemu: vhd
	qemu-system-i386 vhd -s -S &

kill-qemu:
	pkill -9 qemu

# 1 - create vhd
# 2 - partition vhd
# 3 - list partitions
# 4 - create a file system on vhd
# 5 - install the grab bootloader on vhd
vhd: vhd.1 vhd.2 vhd.3 vhd.4 vhd.5

vhd.1:
	dd if=/dev/zero of=vhd bs=512 count=1024

vhd.2:
	docker start $(CONTAINER)
	docker exec $(CONTAINER) bash -c "printf \"n\np\n1\n65\n1023\na\nw\n\" | fdisk /host/$(shell pwd)/vhd"

vhd.3:
	docker exec $(CONTAINER) bash -c "fdisk -l /host/$(shell pwd)/vhd"

vhd.4: mkfs
	$(MKFS)/mkfs vhd 1

vhd.5: grab
	dd if=$(GRAB)/stage1.bin of=vhd bs=1 count=$(shell echo $$((512-2-16*4))) conv=notrunc
	dd if=$(GRAB)/stage2.bin of=vhd bs=512 count=63 seek=1 conv=notrunc

docker:
	docker run -d -it -v $(shell pwd):/host/$(shell pwd) --name $(CONTAINER) --platform linux/amd64 ubuntu:latest
	docker exec $(CONTAINER) bash -c "apt update; apt -y install fdisk"

grab:
	make -C $(GRAB)

mkfs:
	make -C $(MKFS)

clean:
	make -C $(GRAB) clean
	make -C $(MKFS) clean
	-rm vhd *.tmp
