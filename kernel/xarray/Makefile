KERNEL_TREE_PATH?=/lib/modules/$(shell uname -r)/build

EXTRA_CFLAGS = -DDEBUG

obj-m += xarray.o

all: xarray.ko

xarray.ko: xarray.c
	make -C $(KERNEL_TREE_PATH) M=$(PWD) modules

clean:
	make -C $(KERNEL_TREE_PATH) M=$(PWD) clean

.PHONY: all clean
