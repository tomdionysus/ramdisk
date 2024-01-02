# ramdisk
An experimental/teaching project to create a linux block driver kernel module for a ramdisk.

This is a Linux 6.x.x+ kernel module that implements an in-memory blockstore, as a device `/dev/ramdisk`. The device is 100Mb in size.

This is based on [Oleg Kutkov](https://olegkutkov.me/)'s work from 2020 - [Linux block device driver](https://olegkutkov.me/2020/02/10/linux-block-device-driver/) - however there have been significant changes to the way the 6.x.x kernel handles queue allocation and registration, and his excellent example no longer compiles for this kernel version. 

I'm hoping this might be useful as a starting point if you're writing your own block devices for this version of the linux kernel.  

## Build

Please follow the instructions [for your distro](docs/build_distros.md).

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

