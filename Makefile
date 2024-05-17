.PHONY: gdb _gdb qemu clean kill clean grab mkfs partition list-partitions vhd

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

vhd: vhd.1 vhd.2 vhd.3 vhd.4 vhd.5

# Create vhd
vhd.1:
	dd if=/dev/zero of=vhd bs=512 count=1024

# Partition vhd
vhd.2:
	docker start $(CONTAINER)
	docker exec $(CONTAINER) bash -c "printf \"n\np\n1\n65\n1023\na\nw\n\" | fdisk /host/$(shell pwd)/vhd"

# List partitions
vhd.3:
	docker exec $(CONTAINER) bash -c "fdisk -l /host/$(shell pwd)/vhd"

# Format vhd
vhd.4: mkfs
	$(MKFS)/mkfs vhd 1

# Install grab
vhd.5: grab
	$(GRAB)/grab vhd 1

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
