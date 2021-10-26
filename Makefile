# https://www.kernel.org/doc/html/latest/kbuild/modules.html

ifneq ("$(wildcard /etc/debian_version)", "")
uapi_path := /usr/src/linux-headers-$(shell uname -r)/include/uapi
uapi_path := $(subst amd64,common,$(uapi_path))
else
uapi_path := /usr/src/kernels/$(shell uname -r)/include/uapi/
endif

cc-flags-y := -DDEBUG
obj-m := pim_swap.o
pim_swap-y := swap.o alloc_static.o

ifeq ($(KERNELRELEASE),)
KDIR ?= /usr/src/linux-headers-`uname -r`

default:
	make api_install
	make -C $(KDIR) M=$$PWD
endif # KERNELRELEASE

clean:
	rm -f *.o *.ko *.a *.order *.mod.c *.symvers *.mod

tar:
	tar cjf pimswap-1.0.tar.bz2 *.c Makefile Kconfig

api_install:
	sudo install -d $(uapi_path)/dpu
	sudo install -m 644 uapi/dpu.h uapi/dpu_management.h uapi/dpu_memory.h uapi/dpu_runner.h $(uapi_path)/dpu/

api_clean:
	sudo rm -rf $(uapi_path)/dpu
