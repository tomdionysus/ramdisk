# ramdisk
An experimental/teaching project to create a linux block driver kernel module for a ramdisk.

## Build tools, linux headers, kernel module tools

```bash
apt install build-essential linux-headers-6.2.0-39-generic kmod
```

## Building

```bash
make
make install
```

## Load Kernel module

```bash
make load
```

## Unload Kernel module

```bash
make unload
```

