#KERNEL_TREE_PATH?=/lib/modules/$(shell uname -r)/build
KERNEL_TREE_PATH?=$(HOME)/kerndev/kernels/linux

EXTRA_CFLAGS = -DDEBUG

obj-m += fragment2.o

all: fragment2.ko

fragment2.ko: fragment2.c
	make -C $(KERNEL_TREE_PATH) M=$(PWD) modules

clean:
	make -C $(KERNEL_TREE_PATH) M=$(PWD) clean

.PHONY: all clean
