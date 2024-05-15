.section .text
.globl _test

_test:
.fill 32256-(.-_test)-2,1,0x90
jmp .
