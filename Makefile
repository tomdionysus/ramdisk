BINARY     := test_ramdisk
KERNEL      := /lib/modules/$(shell uname -r)/build
ARCH        := x86
C_FLAGS     := -Wall
KMOD_DIR    := $(shell pwd)
TARGET_PATH := /lib/modules/$(shell uname -r)/kernel/drivers/char
BUILD_DIR   := $(KMOD_DIR)/build

ccflags-y += $(C_FLAGS)

obj-m += $(BUILD_DIR)/$(BINARY).o
$(BINARY)-y := ramdisk.o

$(BUILD_DIR)/$(BINARY).ko:
	make -C $(KERNEL) M=$(BUILD_DIR) modules

src/ramdisk.o: src/ramdisk.c
	$(MAKE) -C $(KERNEL) M=$(KMOD_DIR) src

install:
	cp $(BUILD_DIR)/$(BINARY).ko $(TARGET_PATH)
	depmod -a

clean:
	rm -rf $(BUILD_DIR)/*.ko
	rm -rf $(BUILD_DIR)/*.o
	rm -rf $(KMOD_DIR)/src/*.o
