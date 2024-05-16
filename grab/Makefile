all: test.bin boot.elf boot.bin

test.bin: test.o
	x86_64-elf-ld -static -Ttext 0x0 -o test.bin -e _test --oformat binary test.o -g

boot.elf: boot.o
	x86_64-elf-ld -static -Ttext 0x0 -o boot.elf -e _start boot.o -g

boot.bin: boot.o
	x86_64-elf-ld -static -Ttext 0x0 -o boot.bin -e _start --oformat binary boot.o -g

test.o: test.S
	x86_64-elf-gcc -c test.S -o test.o

boot.o: boot.S
	x86_64-elf-gcc -c boot.S -o boot.o

clean:
	rm *.o *.bin *.elf vhd
