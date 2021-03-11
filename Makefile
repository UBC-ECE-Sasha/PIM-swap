CC=gcc

.PHONY: tags

pim-swap:
	make -C $(PWD)/output pimswap-rebuild all

buildroot-setup:
	make -C ../output/ qemu_pim_defconfig O=$(PWD)/output

tags:
	ctags -R -f tags module
