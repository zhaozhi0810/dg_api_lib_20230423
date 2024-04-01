ARCH = arm64
CROSS_COMPILE = aarch64-linux-gnu-

KERNEL_PATH = ~/3399pro/3399_722/rk3399-linux/kernel
#KERNEL_PATH = /home/dazhi/king_rk3399/kernel
EXTRA_CFLAGS += -fno-pie

ccflags-y += -Wall

obj-m += jc_keyboard.o
#ilitek-y +=	jc_keyboard.o

build: kernel_modules
kernel_modules:
	make -C $(KERNEL_PATH) M=$(CURDIR) modules ARCH=${ARCH} CROSS_COMPILE=$(CROSS_COMPILE)
clean:
	make -C $(KERNEL_PATH) M=$(CURDIR) clean ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE)
