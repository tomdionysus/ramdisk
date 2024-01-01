CONFIG_MODULE_SIG=n

BINARY      := test_ramdisk
KERNEL      := /lib/modules/$(shell uname -r)/build
ARCH        := ${uname -m}
C_FLAGS     := -Wall
KMOD_DIR    := $(shell pwd)
TARGET_PATH := /lib/modules/$(shell uname -r)/kernel/drivers/char

OBJECTS := ramdisk.o

ccflags-y += $(C_FLAGS)

obj-m += $(BINARY).o

$(BINARY)-y := $(OBJECTS)

$(BINARY).ko:
	make -C $(KERNEL) M=$(KMOD_DIR) modules

install:
	cp $(BINARY).ko $(TARGET_PATH)

load:
	modprobe $(BINARY)

remove:
	rm $(TARGET_PATH)/$(BINARY).ko
	
unload:
	rmmod -fv $(BINARY)

log:
	echo "7 7" > /proc/sys/kernel/printk
	dmesg -r | grep ramdisk

reset: clean remove unload  

clean:
	rm -f *.ko
	rm -f *.o
	rm -f *.test_*
	rm -f .*.cmd
	rm -f ramdisk.mod*
	rm -f Module.symvers
	rm -f modules.order