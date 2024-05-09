qemu: vhd
	qemu-system-i386 vhd -s -S &

gdb: 
	gdb -ex "target extended-remote localhost:1234" \
	-ex "symbol-file boot.o"

vhd: boot.bin
	dd bs=1M if=/dev/zero of=$@ count=512
	dd bs=512 if=$< of=$@ count=1 conv=notrunc

boot.bin: boot.o
	x86_64-elf-ld -static -Ttext 0x0 -o boot.bin -e init16 --oformat binary boot.o

boot.o: boot.S
	x86_64-elf-as boot.S -o boot.o

clean:
	rm *.o *.bin *.elf vhd