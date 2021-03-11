#!/bin/bash

BUILDROOT_LOCATION=../buildroot

# put all the modified pieces of buildroot in the right spot
cp config/qemu_pim_defconfig $BUILDROOT_LOCATION/configs
cp config/Config.ext.in $BUILDROOT_LOCATION/linux
cp config/linux-ext-pimswap.mk $BUILDROOT_LOCATION/linux
cp config/Config.in $BUILDROOT_LOCATION/package
cp -R config/pimswap $BUILDROOT_LOCATION/package
cp -R config/linux_config $BUILDROOT_LOCATION

# set up the buildroot output directory
make -C $BUILDROOT_LOCATION qemu_pim_defconfig O=$PWD/output

# override the location of the source for the pim_swap package
cp config/local.mk output

# create the virtual swap file
if [[ ! -e swap-1g.qcow2 ]]; then
	qemu-img create -f qcow2 swap-1g.qcow2 1G
fi
