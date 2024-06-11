A simple inode file system implementation.

It provides a single-threaded API exposing the following functions to the user:

- `fs_init()`
- `fs_format()`
- `fs_lookup()`
- `fs_mknod()`
- `fs_geti()`
- `fs_read()`
- `fs_write()`

This implementation is used in my hobby 386 kernel project by both the 'grab' bootloader and the 'mkfs' file system creation tool. The tool runs on the host and is used for formatting fdisk-partitioned VHDs (virtual hard drives) to be used as QEMU IDE drives. The implementation strictly follows the file system specification in `kernel/fs.h` and is designed to be the bare minimum—no failure recovery, no concurrent accesses allowed, no caching (i.e., very inefficient but correct)—for the purposes stated above. It differs from the kernel file system implementation that focuses on recovery and concurrency.

Since it is shared by and compiled along with both the host machine (whether it be x86_64, ARM, etc.—whichever machine one builds the projects on) and the guest machine (i386 emulated by QEMU), `fs_init()` requires the user of this file system implementation to provide three parameters defining the methods for disk operations and error display. Additionally, a CPP (C Preprocessor) macro is required and checked against to indicate the compilation target. If the target is the guest system (i386), then the macro `BUILD_TARGET_386` should be defined. Conversely, the macro `BUILD_TARGET_HOST` should be defined if compiled for the host mkfs tool. This ensures that the header files are included correctly since standard C headers can be included for the host build but not for the guest build.

Specifically, the three parameters are pointers to the disk read and write functions and the `printf()` function. For the host build, disk read and write functions can be simply implemented via `read()` and `write()` functions, whereas for the guest build, they are IDE disk read and write functions provided by the IDE driver. Similarly, `printf()` on the host side is just the libc implementation, while on the guest system, it needs to be implemented along with the VGA display driver.
