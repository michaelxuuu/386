.PHONY: gdb gdb-in-new-bash-session \
		qemu kill-qemu \
		grab mkfs \
		make_drive make_drive.1 make_drive.2 make_drive.3 \
		clean

PATHMKFS 		=	./mkfs
PATHGRAB 		=	./grab
PATHFS   		=	./fs
CONTAINER		=	fdisk
BOOT_DRIVES 	=	drive0
NONBOOT_DRIVES 	= 	drive1 drive2 drive3
DRIVES 			= 	$(BOOT_DRIVES) $(NONBOOT_DRIVES)
DISK_GEO_C		= 	1
DISK_GEO_H		= 	16
DISK_GEO_S		= 	63
SRCGRAB			= 	$(wildcard $(PATHGRAB)/*.c $(PATHFS)/*.c)
SRCMKFS			= 	$(wildcard $(PATHMKFS)/*.c $(PATHFS)/*.c)

gdb: qemu-serial gdb-in-new-bash-session kill-qemu

gdb-in-new-bash-session:
	echo 'x86_64-elf-gdb -ix .gdb/gdbinit && exit' > init.tmp
	bash --init-file init.tmp

qemu-serial: $(DRIVES)
	qemu-system-i386 \
		-drive file=drive0,if=ide,index=0,media=disk,format=raw \
		-drive file=drive1,if=ide,index=1,media=disk,format=raw \
		-drive file=drive2,if=ide,index=2,media=disk,format=raw \
		-drive file=drive3,if=ide,index=3,media=disk,format=raw \
		-s -S &

kill-qemu:
	pkill -9 qemu

.DELETE_ON_ERROR:
$(DRIVES): $(SRCGRAB) $(SRCMKFS) | make_grab make_mkfs
# create drives
	@echo "Creating dirves: $(DRIVES)..."
	@$(foreach drive,$(DRIVES), \
		dd if=/dev/zero of=$(drive) bs=512 count=$(shell echo $$(( $(DISK_GEO_C) * $(DISK_GEO_H) * $(DISK_GEO_S) ))) &> /dev/zero; \
		$(eval DISK_GEO_H := $(shell echo $$(($(DISK_GEO_H) / 2)))) \
	)
# partition dirves
	@docker start $(CONTAINER) > /dev/null
# partition the bootable drives, reserving the first 64 sectors for the bootlaoder,
# leaving a 63 sector post-mbr gap
	@echo "Partitioning bootable drives: $(BOOT_DRIVES)..."
	@$(foreach drive,$(BOOT_DRIVES), \
		docker exec $(CONTAINER) bash -c "printf \"n\np\n1\n65\n\na\nw\n\" | fdisk /host/$(shell pwd)/$(drive) &> /dev/zero "; \
	)
# partition the rest of the drives (without reserving space for post-mbr gaps, aka. the first partitions immediately follows the mbr)
	@echo "Partitioning non-bootable drives: $(NONBOOT_DRIVES)..."
	@$(foreach drive,$(NONBOOT_DRIVES), \
		docker exec $(CONTAINER) bash -c "printf \"n\np\n1\n\n\n\nw\n\" | fdisk /host/$(shell pwd)/$(drive) &> /dev/zero"; \
	)
# list all drives and their partitions
	@echo "Drive info:"
	@$(foreach drive,$(DRIVES), \
		(echo "------------------------------------------------------------------------------------------------------" \
		&& docker exec $(CONTAINER) bash -c "fdisk -x /host/$(shell pwd)/$(drive) | tail -n 2"); \
	)
# create a file system on all bootable drives
	@$(foreach drive,$(BOOT_DRIVES), \
		(echo "Formatting: $(drive)..." && \
		echo "mkdir /boot\n mkdir /home\n quit" | $(PATHMKFS)/mkfs drive0 1 &> /dev/zero); \
	)
# install the grab on all bootable drives
	@$(foreach drive,$(BOOT_DRIVES), \
		(echo "Installing grab to $(drive)" && \
			dd if=$(PATHGRAB)/stage1.bin of=$(drive) bs=1 count=$(shell echo $$((512-2-16*4))) conv=notrunc && \
			dd if=$(PATHGRAB)/stage2.bin of=$(drive) bs=512 count=63 seek=1 conv=notrunc); \
	)

docker:
	docker run -d -it -v $(shell pwd):/host/$(shell pwd) --name $(CONTAINER) --platform linux/amd64 ubuntu:latest
	docker exec $(CONTAINER) bash -c "apt update; apt -y install fdisk"

make_grab:
	make -C $(PATHGRAB)

make_mkfs:
	make -C $(PATHMKFS)

clean:
	make -C $(PATHGRAB) clean
	make -C $(PATHMKFS) clean
	-rm vhd *.tmp drive* vhd*
