#!/bin/bash

TARGET=$1

# add /dev/sda as a swap device
# this device must be passed on the QEMU command line, and already formatted
# as a swap device with 'mkswap /dev/sda'
cat ${BUILD_DIR}/pimswap-custom/fstab >> ${BASE_DIR}/target/etc/fstab
