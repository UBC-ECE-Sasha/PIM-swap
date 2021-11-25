# https://www.kernel.org/doc/html/latest/kbuild/modules.html

ccflags-y := -DDEBUG
ccflags-y += -I$(PWD)/../upmem_driver-2021.3.0/uapi
obj-m := pim_swap.o
pim_swap-y := swap.o dpu.o alloc_static.o

ifeq ($(KERNELRELEASE),)
KDIR ?= /usr/src/linux-headers-`uname -r`

default:
	make -C $(KDIR) M=$$PWD
endif # KERNELRELEASE

clean:
	rm -f *.o *.ko *.a *.order *.mod.c *.symvers *.mod

tar:
	tar cjf pimswap-1.0.tar.bz2 *.c Makefile Kconfig
