#KERNEL_TREE_PATH?=/lib/modules/$(shell uname -r)/build
KERNEL_TREE_PATH?=$(HOME)/kerndev/kernels/linux-mm

EXTRA_CFLAGS = -DDEBUG

obj-m += fragment.o

all: fragment.ko

fragment.ko: fragment.c
	make -C $(KERNEL_TREE_PATH) M=$(PWD) modules

clean:
	make -C $(KERNEL_TREE_PATH) M=$(PWD) clean

.PHONY: all clean
