grab: stage1.bin stage2.bin stage1.elf

stage2.bin: stage2.o
	x86_64-elf-ld -static -Ttext 0x0 -o stage2.bin -e _test --oformat binary stage2.o -g

stage1.elf: stage1.o
	x86_64-elf-ld -static -Ttext 0x0 -o stage1.elf -e _start stage1.o -g

stage1.bin: stage1.o
	x86_64-elf-ld -static -Ttext 0x0 -o stage1.bin -e _start --oformat binary stage1.o -g

stage2.o: stage2.S
	x86_64-elf-gcc -c stage2.S -o stage2.o

stage1.o: stage1.S
	x86_64-elf-gcc -c stage1.S -o stage1.o

clean:
	-rm *.o *.bin *.elf vhd
