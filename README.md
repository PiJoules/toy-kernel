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

# Making a bootable cdrom image that also uses graphics mode.
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
$ qemu-system-i386 -kernel kernel -initrd initrd.vfs -nographic
OR
$ qemu-system-i386 -kernel kernel -nographic
```

Exit QEMU with `Alt+2, type quit, ENTER`.

Note that launching QEMU with `-nographic` can do some weird thing

#### Graphics Modes

TODO: Launching in either VGA or graphics mode may not work anymore. We should
properly support these through user space programs.

Previously, we could display a screen when excluding `-nographic` on which
characters would be rendered, but those should be migrated to use programs that
do the printing instead of building it into the kernel.

```sh
# Just launch the kernel without any initial ramdisk.
$ qemu-system-i386 -display curses -kernel kernel

# Launch the kernel with a ramdisk.
$ qemu-system-i386 -display curses -kernel kernel -initrd initrd.vfs

# Launch in graphics mode where pixels are printed to the screen.
$ qemu-system-i386 -cdrom myos.iso
```

#### Extra QEMU Options

- `-serial file:serial.log`
  - Writes to COM1 will be written to `serial.log`. This is not needed when
    `-nographic` is used.

### TODO: Real hardware

## Tests

### initrd

```sh
$ python3 initrd.py test
```

## Formatting

```sh
# Run from root directory
$ clang-format -style=file -i  src/*.cpp src/include/*.h usersdk/*.c
$ yapf -i --style="{based_on_style: google, indent_width: 2}" *.py
```

## TODOs + FIXMEs

`grep` for them.

## Resources

- QEMU
  - https://www.qemu.org/docs/master/system/qemu-manpage.html
