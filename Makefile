.PHONY: load unload ipconfig33 ipconfig34 ipclean

KSRC=/lib/modules/$(shell uname -r)/build
KOBJ=/lib/modules/$(shell uname -r)/build

TEST_SRC?=$(shell pwd)
OFA_DIR=/usr/src/ofa_kernel

OFA = $(shell test -d $(OFA_DIR)/default && echo $(OFA_DIR)/default || (test -d /var/lib/dkms/mlnx-ofed-kernel/ && ls -d /var/lib/dkms/mlnx-ofed-kernel/*/build) || echo $(OFA_DIR))
ifneq ($(shell test -d $(OFA) && echo YES || echo ""),)
	include $(OFA)/configure.mk.kernel
	INCLUDE_COMPAT = -include $(OFA)/include/linux/compat-2.6.h
endif

EXTRA_CFLAGS += -DLINUX -D__KERNEL__ -DMODULE -O2 -pipe -Wall
EXTRA_CFLAGS += $(shell echo $(BACKPORT_INCLUDES) | sed -e 's@/var/tmp/OFED_topdir/BUILD@/usr/src@')
EXTRA_CFLAGS += $(shell [ -f $(KOBJ)/include/linux/modversions.h ] && \
            echo "-DMODVERSIONS -DEXPORT_SYMTAB \
                  -include $(KSRC)/include/linux/modversions.h")
EXTRA_CFLAGS += $(shell [ -f $(KOBJ)/include/config/modversions.h ] && \
            echo "-DMODVERSIONS -DEXPORT_SYMTAB \
                  -include $(KSRC)/include/config/modversions.h")

autoconf_h=$(shell /bin/ls -1 $(KSRC)/include/*/autoconf.h 2> /dev/null | head -1)
kconfig_h=$(shell /bin/ls -1 $(KSRC)/include/*/kconfig.h 2> /dev/null | head -1)

ifneq ($(kconfig_h),)
KCONFIG_H = -include $(kconfig_h)
endif

ofa_autoconf_h=$(shell /bin/ls -1 $(OFA)/include/*/autoconf.h 2> /dev/null | head -1)
ifneq ($(ofa_autoconf_h),)
OFA_AUTOCONF_H = -include $(ofa_autoconf_h)
endif

OBJ := rdma_init
obj-m := $(OBJ).o
rdma_mod-y := rdma_init.o

default:
	-cp -f $(OFA)/Module*.symvers $(TEST_SRC)/Module.symvers
	make -C $(KSRC) O=$(KOBJ) SUBDIRS=$(shell pwd) \
		LINUXINCLUDE=' \
		-D__OFED_BUILD__ \
		$(EXTRA_CFLAGS) \
		-include $(autoconf_h) \
		$(OFA_AUTOCONF_H) \
		$(KCONFIG_H) \
		-I$(OFA)/include \
		$(INCLUDE_COMPAT) \
		$$(if $$(CONFIG_XEN),-D__XEN_INTERFACE_VERSION__=$$(CONFIG_XEN_INTERFACE_VERSION)) \
		$$(if $$(CONFIG_XEN),-I$$(srctree)/arch/x86/include/mach-xen) \
		-I$$(srctree)/arch/$$(SRCARCH)/include \
		-Iarch/$$(SRCARCH)/include/generated \
		-Iinclude \
		-I$$(srctree)/arch/$$(SRCARCH)/include/uapi \
		-Iarch/$$(SRCARCH)/include/generated/uapi \
		-I$$(srctree)/include \
		-I$$(srctree)/include/uapi \
		-Iinclude/generated/uapi \
		$$(if $$(KBUILD_SRC),-Iinclude2 -I$$(srctree)/include) \
		-I$$(srctree)/arch/$$(SRCARCH)/include \
		-Iarch/$$(SRCARCH)/include/generated \
		' \
		modulesymfile=$(TEST_SRC)/Module.symvers \
		modules

# install:
# 	make -C $(KSRC) O=$(KOBJ) SUBDIRS=$(shell pwd) modules_install
# 	depmod -a

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	# rm -f *.o
	# rm -f *.ko
	# rm -f rdma_krping.mod.c
	# rm -f Module*.symvers

load:
	insmod $(OBJ).ko

unload:
	rmmod -f $(OBJ).ko


ipconfig33:
	echo 'enp130s0f0 --> 192.168.1.33'
	ifconfig enp130s0f0 inet 192.168.1.33
	echo 'enp130s0f1 --> 192.168.1.35'
	ifconfig enp130s0f1 inet 192.168.1.35

ipconfig34:
	echo 'enp130s0f0 --> 192.168.1.34'
	ifconfig enp130s0f0 inet 192.168.1.34
	echo 'enp130s0f1 --> 192.168.1.36'
	ifconfig enp130s0f1 inet 192.168.1.36

ipclean:
	echo 'flush enp130s0f0'
	ip addr flush enp130s0f0
	echo 'flush enp130s0f1'
	ip addr flush enp130s0f1
