-include ../common.mk

SRC_STAGE2 = $(wildcard *.c ../fs/*.c)
OBJ_STAGE2 = $(SRC_STAGE2:.c=.o)

INCLUDE = -I../kernel/include -I../fs/ -I../grab/include

all: stage1.bin stage2.bin stage1.elf stage2.elf

stage2.bin: stage2.elf
	$(OBJCOPY) -S -O binary $^ $@

stage2.elf: $(OBJ_STAGE2)
	$(LD) $(FLAGS_LD) -Tstage2.ld -o $@ $^

%.o: %.c
	$(CC) $(INCLUDE) $(FLAGS_CC) -DBUILD_TARGET_386 -c $< -o $@

stage1.bin: stage1.elf
	$(OBJCOPY) -S -O binary $^ $@

stage1.elf: stage1.o
	$(LD) $(FLAGS_LD) -Ttext 0x0 -e _start -o $@ $^

stage1.o: stage1.S
	$(CC) $(INCLUDE) $(FLAGS_CC) -c -o $@ $^

clean:
	-rm *.o *.bin *.elf $(OBJ_STAGE2)
