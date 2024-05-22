// C wrappers of inline asm that reads and writes to i/o ports
// i/o port addresses are 16 bits
// 

#include <types.h>

uint8_t inb(uint16_t port) {
    uint8_t byte;
    asm ("inb %1, %0" : "=al"(byte) : "d"(port));
    return byte;
}

void outb(uint8_t byte, uint16_t port) {
    asm ("outb %0, %1" : : "al"(byte), "d"(port));
}

uint32_t inl(uint16_t port) {
    uint32_t byte;
    asm ("inl %1, %0" : "=eax"(byte) : "d"(port));
    return byte;
}

void outl(uint32_t dword, uint16_t port) {
    asm ("outl %0, %1" : : "eax"(dword), "d"(port));
}

