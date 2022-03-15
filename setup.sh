#!/bin/bash

# Exit on error
set -e

ENV_FILE="./.env"

# Source env variables
[ -f "${ENV_FILE}" ] && . "${ENV_FILE}"

BUILDROOT_LOCATION=../buildroot
OUTPUT_DIR=output

# put all the modified pieces of buildroot in the right spot
cp config/qemu_pim_defconfig $BUILDROOT_LOCATION/configs
cp config/Config.ext.in $BUILDROOT_LOCATION/linux
cp config/linux-ext-pimswap.mk $BUILDROOT_LOCATION/linux
cp config/Config.in $BUILDROOT_LOCATION/package
cp -R config/{pimswap,packages/upmem_driver} $BUILDROOT_LOCATION/package
cp -R config/linux_config $BUILDROOT_LOCATION
cp -R config/rootfs_overlay $BUILDROOT_LOCATION
cp bench/log_mem.sh $BUILDROOT_LOCATION/rootfs_overlay/root/
cp -R config/upmem $BUILDROOT_LOCATION/board

# set up the buildroot output directory
make -C $BUILDROOT_LOCATION qemu_pim_defconfig O=$PWD/$OUTPUT_DIR

# override the location of the source for the pim_swap package
cp config/local.mk $OUTPUT_DIR

if [[ ! -e swap-1g.raw ]]; then
	echo "create the virtual swap file with 'sudo ./swap_setup.sh'"
fi
