# ramdisk

## Build Evnironment

Your build enviromnent and dependences will depend on the linux distro you are using. In gnereal, you will need:
* **build-essentials** or the equivalent, which is a C compiler and associated support tools
* **kernel headers** for the specific version of the linux kernel you are using
* **kernel tools** usually *kmod* or an equivalent package, for loading, querying and unloading kernel modules.

## Dependencies

For convenience, you can use your package manager in your distro to install as follows:

### Ubuntu 22.04 LTS

```bash
apt install build-essential linux-headers-$(uname -r) kmod 
```
### Fedora 39

```bash
dnf groupinstall "Development Tools"
dnf install kernel-devel kmod
```

### Alpine Linux 

```bash
apk add build-base linux-headers alpine-sdk linux-lts-dev kmod lsblk e2fsprogs
```

### Rapsberry OS

```bash
apt install build-essential raspberrypi-kernel-headers kmod 
```

