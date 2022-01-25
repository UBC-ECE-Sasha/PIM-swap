CC=gcc
ccflags-y := -I/home/shaurya/PIM-swap/output/build/upmem_driver-2021.3.0/
.PHONY: tags

pim-swap:
	make -C $(PWD)/output pimswap-rebuild all

buildroot-setup:
	make -C ../output/ qemu_pim_defconfig O=$(PWD)/output

tags:
	ctags -R -f tags module

graphs:
	all-graphs.sh
