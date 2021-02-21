`font.psf` was copied from the [soso](https://github.com/ozkl/soso) repository,
which was referenced many times when building this. Thanks to the devs on them.
:)

## Requirements

- Clang 9.0.0
- ld 2.34
- CMake 3.15.4

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
$ ninja initrd.vfs

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
$ qemu-system-i386 -kernel kernel/kernel -initrd initrd.vfs -nographic
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

### Initial Ramdisk Format

```sh
$ python3 initrd.py test
```

## Formatting

```sh
# Run from root directory
# FIXME: This doesn't capture the headers in libcxx/ that don't end with .h
$ clang-format -style=file -i  $(find . -name '*.h') $(find . -name '*.cpp')
$ yapf -i --style="{based_on_style: google, indent_width: 2}" *.py
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
