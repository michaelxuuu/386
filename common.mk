PATH_KERNEL = ../kernel
PATH_FS = ../fs
PATH_MKFS = ../mkfs
PATH_GRAB = ../grab
INCLUDE = -I$(PATH_FS) -I$(PATH_GRAB)/include -I$(PATH_KERNEL)/include
FLAGS_LD = -g -static --fatal-warnings -melf_i386
FLAGS_CC = -nostdinc \
	-fno-strict-aliasing -fno-builtin -fno-stack-protector -fno-omit-frame-pointer \
	-fno-delete-null-pointer-checks -fwrapv \
	-fno-pic \
	--std=gnu99 \
	-D__STDC_NO_ATOMICS__ \
	-Wall -Werror \
	-fno-aggressive-loop-optimizations \
	-g -gstrict-dwarf -O0 -m32 \
	-Wno-unused-variable -Wno-unused-function \
	-c -mno-sse \

LD = x86_64-elf-ld
CC = x86_64-elf-gcc
OBJCOPY = x86_64-elf-objcopy
