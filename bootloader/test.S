.section .text
.globl _test

_test:
.fill 32256-(.-_test)-2-4,1,0x90
jmp .
nop
nop
nop
nop
