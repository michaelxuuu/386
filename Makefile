run: qemu kill

qemu: vhd boot.elf
	qemu-system-i386 vhd -s -S

kill:
	pkill -9 qemu

gdb:
	x86_64-elf-gdb -ix ".gdb/gdbinit"

vhd: boot.bin test.bin
	dd if=/dev/urandom of=vhd bs=1K count=32
	dd bs=512 if=boot.bin of=$@ count=1 conv=notrunc
	dd bs=512 if=test.bin of=$@ seek=1 count=63 conv=notrunc

test.bin: test.o
	x86_64-elf-ld -static -Ttext 0x0 -o test.bin -e _test --oformat binary test.o -g

boot.elf: boot.o
	x86_64-elf-ld -static -Ttext 0x0 -o boot.elf -e _start boot.o -g

boot.bin: boot.o
	x86_64-elf-ld -static -Ttext 0x0 -o boot.bin -e _start --oformat binary boot.o -g

test.o: test.S
	x86_64-elf-as test.S -o test.o

boot.o: boot.S
	x86_64-elf-as boot.S -o boot.o

clean:
	rm *.o *.bin *.elf vhd