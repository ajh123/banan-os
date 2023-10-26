![license](https://img.shields.io/github/license/bananymous/banan-os)

# banan-os

This is my hobby operating system written in C++. Currently supports only x86_64 architecture. We have a ext2 filesystem, basic ramfs, IDE disk drivers in ATA PIO mode, ATA AHCI drivers, userspace processes, executable loading from ELF format, linear VBE graphics and multithreaded processing on single core.

![screenshot from qemu running banan-os](assets/banan-os.png)

## Code structure

Each major component and library has its own subdirectory (kernel, userspace, libc, ...). Each directory contains directory *include*, which has **all** of the header files of the component. Every header is included by its absolute path.

## Building

There does not exist a complete list of needed packages for building. From the top of my head I can say that *cmake*, *ninja*, *make*, *grub*, *rsync* and emulator (*qemu* or *bochs*) are needed.

To build the toolchain for this os. You can run the following command.
> ***NOTE:*** The following step has to be done only once. This might take a long time since we are compiling binutils and gcc.
```sh
./script/build.sh toolchain
```

To build the os itself you can run one of the following commands. You will need root access since the sysroot has "proper" permissions.
```sh
./script/build.sh qemu
./script/build.sh qemu-nographic
./script/build.sh qemu-debug
./script/build.sh bochs
```

You can also build the kernel or disk image without running it:
```sh
./script/build.sh kernel
./script/build.sh image
```

If you have corrupted your disk image or want to create new one, you can either manually delete *build/banan-os.img* and build system will automatically create you a new one or you can run the following command.
```sh
./script/build.sh image-full
```

> ***NOTE*** ```ninja clean``` has to be ran with root permissions, since it deletes from the banan-so sysroot.

### Contributing

Currently I don't accept contributions to this repository unless explicitly told otherwise. This is a learning project for me and I want to do everything myself. Feel free to fork/clone this repo and tinker with it yourself.
