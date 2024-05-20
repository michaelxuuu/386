-include ../include.mk

SRC_STAGE2 = $(wildcard $(PATH_KERNEL)/vga.c $(PATH_KERNEL)/pio.c $(PATH_KERNEL)/util.c stage2.c)
OBJ_STAGE2 = $(SRC_STAGE2:.c=.o)

all: stage1.bin stage2.bin stage1.elf stage2.elf

stage2.bin: stage2.elf
	$(OBJCOPY) -S -O binary $^ $@

stage2.elf: $(OBJ_STAGE2)
	$(LD) $(FLAGS_LD) -Tstage2.ld -o $@ $^

%.o: %.c
	$(CC) $(INCLUDE) $(FLAGS_CC) -c $< -o $@

stage1.bin: stage1.elf
	$(OBJCOPY) -S -O binary $^ $@

stage1.elf: stage1.o
	$(LD) $(FLAGS_LD) -Ttext 0x0 -e _start -o $@ $^

stage1.o: stage1.S
	$(CC) $(FLAGS_CC) -c -o $@ $^

clean:
	-rm *.o *.bin *.elf $(OBJ_STAGE2)
