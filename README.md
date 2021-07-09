# Unnamed Toy Kernel

`font.psf` was copied from the [soso](https://github.com/ozkl/soso) repository,
which was referenced many times when building this. Thanks to the devs on them.
:)

## (Minimum) Requirements

- Clang 11.0.0
  - `clang` is nice because it doesn't require building a full cross-compiler
    toolchain.
- lld 11.0.0
- ld 2.35.2
  - FIXME: Turns out I've been using the system linker for flat binaries. I
    should get rid of this and just use lld.
- CMake 3.15.4
- tar 1.32
  - Used for making the filesystem used by userboot.

### Grub Requirements (Optional)

- grub-file 2.04-5
- grub-mkrescue 2.04-5

## Build

```sh
$ mkdir build
$ cd build
$ cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang

# Just make the kernel.
$ ninja  # or `ninja kernel`

# Make the ramdisk which will contain some user code.
$ ninja initrd

# Optional: Make a bootable cdrom image that also contains the initial ramdisk.
$ ninja myos.iso
```

## Running

### QEMU

```sh
# RECOMMENDED: Running without displaying graphics. Logs and interaction are
# done exclusively through emulated serial ports on QEMU. Running in this mode
# does not require any graphics and text is completely done through the user
# console. This allows for kernel logs to be piped to user commands for
# automation.
$ qemu-system-i386 -cdrom myos.iso -nographic
OR
$ qemu-system-i386 -kernel kernel/kernel -initrd initrd -nographic
OR
$ qemu-system-i386 -kernel kernel/kernel -nographic
```

Exit QEMU with `Alt+2, type quit, ENTER`.

Note that launching QEMU with `-nographic` can mess up the terminal on some
shells. Use `reset` after exiting QEMU to fix this.

#### Extra QEMU Options

- `-serial file:serial.log`
  - Writes to COM1 will be written to `serial.log`. This is not needed when
    `-nographic` is used.
- `-no-reboot`
  - Do not reboot on a kernel crash.

### TODO: Real hardware

## Tests

Kernel tests are run automatically at startup.

If userboot is provided, then once Userboot Stage 2 is launched and a shell
opens up, then you can type `runtests` to run userspace tests.

## Userboot

If an initial ramdisk is provided, the kernel will jump to the start of the
ramdisk and execute instructions from there in userspace. This program is called
the userboot. The initial ramdisk is formatted such that the first stage of
userboot, which is a flat binary, is at the very start of the initial ramdisk.
Immediately following it is a USTART-formatted filesystem that userboot parses.

TODO: Move the userboot directory into its own repo.

## Formatting

```sh
# Run from root directory
$ clang-format -style=file -i  $(find . -name '*.h') $(find . -name '*.cpp') libcxx/include/*
$ yapf -i --style="{indent_width: 2}" userboot/make_initrd.py
```

## TODOs + FIXMEs

`grep` for them.

## Debugging

### Flat User Binaries

```sh
$ objdump -b binary user_program.bin -D -m i386 |& less
```

## Repo Structure

Everything in each of the top-level project directories should effectively be
independent of each other (the kernel should be able to be built independently
of userboot, and userboot should not be hardcoded to depend on *this* libc).

Subprojects in each subdirectory can depend on each other, but should be easily
substituable for similar tools (userboot can be substituted for any other
program that the kernel can run on startup, and userboot can use any other libc
that can run on this kernel).

## Resources

- QEMU
  - https://www.qemu.org/docs/master/system/qemu-manpage.html
