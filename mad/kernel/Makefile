#KERNEL_TREE_PATH?=/lib/modules/$(shell uname -r)/build
KERNEL_TREE_PATH?=/home/lorenzo/kerndev/kernels/linux-mm

EXTRA_CFLAGS = -DDEBUG

obj-m += mad.o

all: mad.ko

mad.ko: mad.c
	make -C $(KERNEL_TREE_PATH) M=$(PWD) modules

clean:
	make -C $(KERNEL_TREE_PATH) M=$(PWD) clean

.PHONY: all clean
