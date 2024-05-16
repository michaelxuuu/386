.PHONY: clean partition partition.1 partition.2 list dockerrun

all: mkfs partition
	./mkfs vhd 4

partition: partition.1 partition.2

partition.1:
	-rm vhd

partition.2: vhd
	docker start c0
	docker exec c0 bash -c "printf \"n\np\n4\n65\n1023\nw\n\" | fdisk /host/Desktop/386/mkfs/vhd"

list:
	docker exec c0 bash -c "fdisk -l /host/Desktop/386/mkfs/vhd"

dockerrun:
	docker run -it -v $HOME:/host --name c0 --platform linux/amd64 ubuntu:latest

mkfs: mkfs.c ../grab/fs.c
	gcc $^ -o $@

vhd:
	dd if=/dev/zero of=$@ bs=512 count=1024

clean:
	rm mkfs vhd
	