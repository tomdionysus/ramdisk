# ramdisk

An experimental/teaching project to create a linux block driver kernel module for a ramdisk - reimplementing [brd](https://docs.kernel.org/admin-guide/blockdev/ramdisk.html).

This is a kernel module that implements an in-memory blockstore, as a device `/dev/ramdisk`. The device is 100Mb in size.

The project is based on [Oleg Kutkov](https://olegkutkov.me/)'s work from 2020 - [Linux block device driver](https://olegkutkov.me/2020/02/10/linux-block-device-driver/).

The Linux block device API varies over different versions of the kernel, please see the following table for the branch which is compatible given your distro:

| Linux Distribution | Version             | Linux Kernel          | Branch                                   |
|--------------------|---------------------|-----------------------|------------------------------------------|
| Ubuntu             | 22.04 LTS           | 5.15.0-91-generic     | [linux-5.15.x](../../tree/linux-5.15.x)  |
| Ubuntu             | 22.04 LTS           | 6.2.0-39-generic      | [linux-6.6.x](../../tree/linux-6.2.x)    |
| Alpine             | 3.19.0              | 6.6.7-0-lts           | [linux-6.6.x](../../tree/linux-6.6.x)    |
| Raspberry Pi OS    | Debian GNU/Linux 11 | 6.1.21-v8+            | [linux-6.1.x](../../tree/linux-6.1.x)    |
| Fedora             | 39                  | 6.5.6-300.fc39.x86_64 | [linux-6.5.x](../../tree/linux-6.5.x)    |
| Debian             | 11                  |                       | *TBC*                                    |
| CentOS             | 9                   |                       | *TBC*                                    |
| openSUSE           | 15.4                |                       | *TBC*                                    |
| Arch Linux         | Rolling Release     |                       | *TBC*                                    |
| Manjaro            | 21.2                |                       | *TBC*                                    |
| Mint               | 20.3                |                       | *TBC*                                    |
    
These branches have been tested only with the specific distros and kernels. If your distro isn't listed, use the branch closest to your Linux kernel version - you can get the linux kernel version of your distro with the following:

```bash
uname -r
```



It is my hope this might be useful as a starting point if you're writing your own block devices.

## Build tools, linux headers, kernel module tools

These vary depending on which distribution you're using - please see the branches in the table above for more info.

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

