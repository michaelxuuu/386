-include ../common.mk

SRC_S = $(wildcard *.c)
SRC_C = $(wildcard *.S)
OBJ = $(SRC_C:.c =.o) $(SRC_S:.S=.o)

INCLUDE = ./include

%.o:%.c
	$(CC) $(INCLUDE) $(FLAGS_CC) $< -o $@

%.o:%.S
	$(CC) $(INCLUDE) $(FLAGS_CC) $< -o $@

