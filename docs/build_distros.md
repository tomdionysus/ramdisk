## Build tools, linux headers, kernel module tools

### Ubuntu 22.04 LTS

### Dependencies

```bash
apt install build-essential linux-headers-$(uname -r) kmod 
```

### Building

```bash
make
make install
depmod -a
make load
```

### Fedora 39

```bash
dnf groupinstall "Development Tools"
dnf install kernel-devel kmod
```

```bash
make
make install
depmod -a
make load
```