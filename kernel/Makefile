-include ../common.mk

SRC_S = $(wildcard *.S)
SRC_C = $(wildcard *.c)
OBJ = $(SRC_C:.c =.o) $(SRC_S:.S=.o)

INCLUDE = -I./include

kernel.bin: kernel.elf
	$(OBJCOPY) -S -O binary $^ $@

kernel.elf: $(OBJ)
	$(LD) $(FLAGS_LD) -Lkernel.ld -o $@ $^

%.o:%.c
	$(CC) $(INCLUDE) $(FLAGS_CC) $< -o $@

%.o:%.S
	$(CC) $(INCLUDE) $(FLAGS_CC) $< -o $@

clean:
	rm *.elf *.bin *.o
