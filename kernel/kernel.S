#include <asm.h>

.code32
.text
.section .text._start
.global _start
.extern kmain

_start:
// This is where the kernel starts.
// We can't assume anything about the bootloader GDT.
// Thus, we must do relative addressing before we set up
// a new GDT for the kernel. Thus, we use Assembly, not C.

// We first load the new GDT
	lgdtl gdt_ptr - _start + 0x100000 // Must use lgdt with suffix "l" to indicate the base field of gdtr should be fully loaded. Also, the address must be calculated manually, as "lgdtl gdt_ptr" would attempt to load from some address above 3G where the code is linked. We should not use any symbols as addresses before we alter the GDT.
	ljmpl $8, $go
go: 
	call kmain

gdt:

//
// Kernel code is linked to 3G to implement higher half kernel
// when paging is abled, meaning the kernel runs in the higher
// "half" of the address space while the user code runs in the
// lower "half" of the address space.
//
// 3G = 3 * 2^(4 * 7) * 2^2 = 3 * 16^7 * 2^2 = 3 * 0x10000000 * 4 = 0xC0000000
//
// However, when being handed control to by the bootloader, paging
// is disabled. Code still runs in the protected mode. So, in order
// to let the code run correctly before paging is enabled, we must
// create GDT entries for the kernel code and data such that all the
// address references above 3G will be wrapped around to be their
// corresponding loactions near and above 1M.
//
// With the kernel code linked to 3G, if we set the GDT base to 1G + 1M
// all address references after consulting the GDT would be converted to
// the original address + 1G + 1M, wrapping it to where the code and data
// is actually stored near 1M in the physical memory.
//
	.word 0, 0, 0, 0
	GDTE(0xfffff, 0x40100000, 1) // kernel code
	GDTE(0xfffff, 0x40100000, 0) // kernel data
gdt_ptr:
gdt_lim:
	.word gdt_ptr - gdt - 1
gdt_base:
	.long gdt - _start + 0x100000 // linear address (GDT translation does not consult GDT! Of course, as it can't consult itself!)
	
