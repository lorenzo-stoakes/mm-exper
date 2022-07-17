KERNEL_TREE_PATH?=/lib/modules/$(shell uname -r)/build

EXTRA_CFLAGS = -DDEBUG

obj-m += exper.o

all: exper.ko

exper.ko: exper.c
	make -C $(KERNEL_TREE_PATH) M=$(PWD) modules

clean:
	make -C $(KERNEL_TREE_PATH) M=$(PWD) clean

.PHONY: all clean
