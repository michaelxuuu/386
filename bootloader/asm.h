// gdt entry format:
///////////////////////
// low -> high
//  |
//  v
// high
///////////////////////
// word - lim[0, 15]
// word - base[0, 15]
// byte - base[16, 23]
// byte - Accessed, Read/Writable, Conforming/Expand, Executable, Type, DPL0, DPL1, Present
// byte - lim[16, 19], Available, 0, 16/32, Granularity
// byte - base[24, 31]

#define GETE(lim, base, exe) \
    .word (lim & 0xffff); \
    .word (base & 0xffff); \
    .byte ((base >> 16) & 0xff); \
    .byte (0b10010010 | (exe << 3)); \
    .byte (((lim >> 16) & 0xf) | (0b1100 << 4)); \
    .byte ((base >> 24) & 0xff)
