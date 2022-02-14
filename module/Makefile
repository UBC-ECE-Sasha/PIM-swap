# https://www.kernel.org/doc/html/latest/kbuild/modules.html

ccflags-y := -DDEBUG
ccflags-y += -I$(PWD)/../upmem_driver-2021.3.0/modules
ccflags-y += -I$(PWD)/../upmem_driver-2021.3.0/modules/uapi
ccflags-y += -I$(PWD)/../upmem_driver-2021.3.0/mappings/fpga_aws
ccflags-y += -I$(PWD)/../upmem_driver-2021.3.0/mappings/fpga_kc705
ccflags-y += -I$(PWD)/../upmem_driver-2021.3.0/mappings/power9
ccflags-y += -I$(PWD)/../upmem_driver-2021.3.0/mappings/xeon_sp


obj-m := pim_swap.o ../upmem_driver-2021.3.0/modules/
pim_swap-y := swap.o 
#pim_swap-y += ../upmem_driver-2021.3.0/modules/dpu_runner.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/dpu_management.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/dpu_memory.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/dpu_region.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/dpu_region_address_translation.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/dpu_rank.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/dpu_rank_sysfs.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/dpu_dax.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/dpu_control_interface.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/dpu_mcu_ci_protocol.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/dpu_region_dev.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/dpu_region_srat.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/dpu_rank_dmi.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/ufi_ci.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/ufi_bit_config.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/ufi_dma_wavegen_config.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/dpu_config.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/dpu_rank_mcu.o
#pim_swap-y += ../upmem_driver-2021.3.0/modules/dpu_power_management.o
#pim_swap-y += ../upmem_driver-2021.3.0/mappings/fpga_kc705/dpu_fpga_kc705_translation.o
#pim_swap-y += ../upmem_driver-2021.3.0/mappings/fpga_kc705/dpu_fpga_kc705_dma_op.o
#pim_swap-y += ../upmem_driver-2021.3.0/mappings/fpga_kc705/dpu_fpga_kc705_spi.o
#pim_swap-y += ../upmem_driver-2021.3.0/mappings/fpga_kc705/dpu_fpga_kc705_fs.o
#pim_swap-y += ../upmem_driver-2021.3.0/mappings/fpga_aws/dpu_fpga_aws_translation.o
#pim_swap-y += ../upmem_driver-2021.3.0/mappings/fpga_aws/dpu_fpga_aws_libxdma.o

ifeq ($(KERNELRELEASE),)
KDIR ?= /usr/src/linux-headers-`uname -r`

default:
	make -C $(KDIR) M=$$PWD 
endif # KERNELRELEASE

clean:
	rm -f *.o *.ko *.a *.order *.mod.c *.symvers *.mod

tar:
	tar cjf pimswap-1.0.tar.bz2 *.c Makefile Kconfig
