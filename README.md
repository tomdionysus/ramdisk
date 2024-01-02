# ramdisk

An experimental/teaching project to create a linux block driver kernel module for a ramdisk.

This is a kernel module that implements an in-memory blockstore, as a device `/dev/ramdisk`. The device is 100Mb in size.

The project is based on [Oleg Kutkov](https://olegkutkov.me/)'s work from 2020 - [Linux block device driver](https://olegkutkov.me/2020/02/10/linux-block-device-driver/).

The Linux block device API varies over different versions of the kernel, please see the following table for the branch which is compatible given your distro:

| Linux Distribution | Version          | Linux Kernel  | Branch                                   |
|--------------------|------------------|---------------|------------------------------------------|
| Ubuntu             | 22.04 LTS        |               |                                          |
| Alpine             | 3.19.0           | 6.6.7-0-lts   | [linux-6.6.x](/ramdisk/tree/linux-6.6.x) |
| Raspbian           |                  |               | *TBC*                                    |
| Fedora             | 36               |               | *TBC*                                    |
| Debian             | 11               |               | *TBC*                                    |
| CentOS             | 9                |               | *TBC*                                    |
| openSUSE           | 15.4             |               | *TBC*                                    |
| Arch Linux         | Rolling Release  |               | *TBC*                                    |
| Manjaro            | 21.2             |               | *TBC*                                    |
| Mint               | 20.3             |               | *TBC*                                    |

These branches have been tested only with the specific distros and kernels. You can get the linux kernel version of your distro with the following:

```bash
uname -r
```

It is my hope this might be useful as a starting point if you're writing your own block devices.

## Build tools, linux headers, kernel module tools

### Alpine Linux 

apk add build-base linux-headers alpine-sdk linux-lts-dev kmod lsblk e2fsprogs
apt install build-essential linux-headers-$(uname -r) kmod 

## Load/Unload Kernel module

```bash
make load
```

```bash
make unload
```

## Example Usage

These commands should show the device at `/dev/ramdisk`, format the ramdisk for ext4, create a mount directoy `/rdtest` and then mount the ramdisk at that mountpoint. The last `lsblk` will show the ramdisk mounted at that path.

```bash
lsblk -l
mkfs.ext4 /dev/ramdisk
mkdir /rdtest
mount /dev/ramdisk /rdtest
lsblk -f
```

## License

ramdisk is licensed under the GNU General Public License v3. Please see [LICENSE](LICENSE).

