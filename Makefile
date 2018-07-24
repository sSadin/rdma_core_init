OBJ := rdma_init
obj-m := $(OBJ).o
#ccflags-y +=-I/usr/src/mlnx-ofed-kernel-4.3/include/
#EXTRA_CFLAGS += -I/usr/src/mlnx-ofed-kernel-4.3/include

.PHONY: load unload

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

load:
	insmod $(OBJ).ko

unload:
	rmmod -f $(OBJ).ko

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
