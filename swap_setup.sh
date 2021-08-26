#!/bin/bash

IMAGE_SIZE=${1:-1} # set to default of 1GB if not passed in command line

set -e

if [ ${EUID} != 0 ]; then
    echo "'$0' must be run as root" 1>&2
    exit 1
fi

SWAP_IMAGE="swap-${IMAGE_SIZE}g.raw"

# Create swap image
qemu-img create -f raw "${SWAP_IMAGE}" ${IMAGE_SIZE}G

# image must be o=rw- on UPMEM servers
chmod 666 "${SWAP_IMAGE}"
chown $USER:$USER "${SWAP_IMAGE}"

loop_dev=$(losetup -f)
if [ -z "${loop_dev}" ]; then
    echo "No free loop devices found"
    exit 1
fi

losetup "${loop_dev}" "${SWAP_IMAGE}"
mkswap "${loop_dev}"
losetup -d "${loop_dev}"
