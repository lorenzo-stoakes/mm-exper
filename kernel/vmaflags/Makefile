KERNEL_TREE_PATH?=/lib/modules/$(shell uname -r)/build

EXTRA_CFLAGS = -DDEBUG

obj-m += vmaflags.o

all: vmaflags.ko

vmaflags.ko: vmaflags.c
	make -C $(KERNEL_TREE_PATH) M=$(PWD) modules

clean:
	make -C $(KERNEL_TREE_PATH) M=$(PWD) clean

.PHONY: all clean
