# https://www.kernel.org/doc/html/latest/kbuild/modules.html

obj-m := pim_swap.o

ifeq ($(KERNELRELEASE),)
KDIR ?= /usr/src/linux-headers-`uname -r`

default:
	make -C $(KDIR) M=$$PWD
endif # KERNELRELEASE

tar:
	tar cjf pimswap-1.0.tar.bz2 pim_swap.c Makefile Kconfig
