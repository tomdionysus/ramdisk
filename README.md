# ramdisk
An experimental/teaching project to create a linux block driver kernel module for a ramdisk.



## Running docker Ubuntu for development
```bash
sudo docker run -it --privileged --rm -v ~/ramdisk_source ubuntu:latest
```


## Build tools, linux headers, kernel module tools

```bash
apt install build-essential linux-headers-6.2.0-39-generic kmod
```

## Alpine linux on VirtualBox

```bash
cat > /etc/apk/repositories << EOF
https://dl-cdn.alpinelinux.org/alpine/v$(cut -d'.' -f1,2 /etc/alpine-release)/main/
https://dl-cdn.alpinelinux.org/alpine/v$(cut -d'.' -f1,2 /etc/alpine-release)/community/
https://dl-cdn.alpinelinux.org/alpine/edge/testing/
EOF
apk update
```