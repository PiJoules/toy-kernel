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
$ cmake .. -G Ninja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
$ ninja
$ qemu-system-i386 -display curses -kernel kernel

# Making a bootable cdrom image (preferred)
$ ninja myos.iso
$ qemu-system-i386 -cdrom myos.iso

# Exit QEMU with Alt+2, type quit, ENTER
```

### QEMU Options

- `-serial file:serial.log`
  - Writes to COM1 will be written to `serial.log`.

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
