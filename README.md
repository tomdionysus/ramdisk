# ramdisk

An experimental/teaching project to create a linux block driver kernel module for a ramdisk - reimplementing [brd](https://docs.kernel.org/admin-guide/blockdev/ramdisk.html).

This is a kernel module that implements an in-memory blockstore, as a device `/dev/ramdisk`. The device is 100Mb in size.

The project is based on [Oleg Kutkov's article from 2020](https://olegkutkov.me/) - [Linux block device driver](https://olegkutkov.me/2020/02/10/linux-block-device-driver/).

The Linux block device API varies over different versions of the kernel, please see the following table for the branch which is compatible given your distro:

| Linux Distribution | Version             | Linux Kernel          | Branch                                   |
|--------------------|---------------------|-----------------------|------------------------------------------|
| Ubuntu             | 22.04 LTS           | 5.15.0-91-generic     | [linux-5.15.x](../../tree/linux-5.15.x)  |
| Ubuntu             | 22.04 LTS           | 6.2.0-39-generic      | [linux-6.2.x](../../tree/linux-6.2.x)    |
| Alpine             | 3.19.0              | 6.6.7-0-lts           | [linux-6.2.x](../../tree/linux-6.2.x)    |
| Raspberry Pi OS    | Debian GNU/Linux 11 | 6.1.21-v8+            | [linux-6.1.x](../../tree/linux-6.1.x)    |
| Fedora             | 39                  | 6.5.6-300.fc39.x86_64 | [linux-6.2.x](../../tree/linux-6.2.x)    |

    
These branches have been tested only with the specific distros and kernels. If your distro isn't listed, use the branch closest to your Linux kernel version - you can get the linux kernel version of your distro with the following:

```bash
uname -r
```

It is my hope this might be useful as a starting point if you're writing your own block devices.

## Status

ramdisk has been tested working in Jan 2024 on the platforms and kernels above, and can be considered BETA.

## Build tools, linux headers, kernel module tools

Please follow the build instructions [for your distro](docs/build_distros.md).

## Example Usage

These commands should show the device at `/dev/ramdisk`, format the ramdisk for ext4, create a mount directoy `/rdtest` and then mount the ramdisk at that mountpoint. The last `lsblk` will show the ramdisk mounted at that path.

```bash
make load
lsblk -l
mkfs.ext4 /dev/ramdisk
mkdir /rdtest
mount /dev/ramdisk /rdtest
lsblk -f
```

## Benchmark

You can measure the peformance of the ramdisk with `dd`. This assumes you mounted it at `/rdtest` as above:

```bash
dd if=/dev/zero of=/rdtest/testfile bs=1M count=50 oflag=direct
```

## License

ramdisk is licensed under the GNU General Public License v3. Please see [LICENSE](LICENSE).

