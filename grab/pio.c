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

// INTEL 80386 PROGRAMMER'S REFERENCE MANUAL 1986
// 8.2.2 Block I/O Instructions
void insl(uint16_t port, void *dest, int cnt)
{
  asm volatile("cld; rep insl" :
               "=D" (dest), "=c" (cnt) :
               "d" (port), "0" (dest), "1" (cnt) : // "0" and "1" tells the compiler "=D" and "=c" are also output registers (meaning they are modified by this instruction!)
               "memory", "cc");
}

void outsl(void *src, uint16_t port, int cnt)
{
  asm volatile("cld; rep outsl" :
               "=S" (src), "=c" (cnt) :
               "d" (port), "0" (src), "1" (cnt) :
               "cc");
}

