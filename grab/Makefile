KERNEL = ../kernel
FS = ../fs
UTILITY = ../utility
INC = -I$(KERNEL)/include -I$(FS) -I$(UTILITY)
STAGE2_SRC = $(wildcard $(KERNEL)/vga.c $(KERNEL)/pio.c $(UTILITY)/*.c stage2.c)
STAGE2_OBJ = $(STAGE2_SRC:.c=.o)

LD_FLAGS = -g -static --fatal-warnings -melf_i386

CFLAGS_COMMON = -nostdinc \
	-fno-strict-aliasing -fno-builtin -fno-stack-protector -fno-omit-frame-pointer \
	-fno-delete-null-pointer-checks -fwrapv \
	-fno-pic \
	--std=gnu99 \
	-D__STDC_NO_ATOMICS__ \
	-Wall -Werror

CFLAGS_GCC = $(CFLAGS_COMMON) \
	-fno-aggressive-loop-optimizations \
	-g -gstrict-dwarf -O0 -m32

grab: stage1.bin stage2.bin stage1.elf stage2.elf

stage2.elf: $(STAGE2_OBJ)
	x86_64-elf-ld $(LD_FLAGS) -Tstage2.lds -o stage2.elf -e start2 $^ -g

stage2.bin: $(STAGE2_OBJ)
	x86_64-elf-ld $(LD_FLAGS) -Tstage2.lds -o stage2.bin -e start2 --oformat binary $^ -g

stage1.elf: stage1.o
	x86_64-elf-ld $(LD_FLAGS) -Ttext 0x0 -o stage1.elf -e _start stage1.o -g

stage1.bin: stage1.o
	x86_64-elf-ld $(LD_FLAGS) -Ttext 0x0 -o stage1.bin -e _start --oformat binary stage1.o -g

%.o: %.c
	x86_64-elf-gcc $(INC) $(CFLAGS_GCC) -c $< -o $@

stage1.o: stage1.S
	x86_64-elf-gcc $(CFLAGS_GCC) -c stage1.S -o stage1.o

clean:
	-rm *.o *.bin *.elf $(STAGE2_OBJ)
