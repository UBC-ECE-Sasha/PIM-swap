#!/bin/sh

set -e

SWAP_IMAGE="swap-1g.raw"

# Create swap image
qemu-img create -f raw "${SWAP_IMAGE}" 1G

# image must be o=rw- on UPMEM servers
chmod 666 "${SWAP_IMAGE}"
chown $USER:$USER "${SWAP_IMAGE}"

loop_dev=$(losetup -f)
if [ -z "${loop_dev}" ]; then
    echo "No free loop devices found, increase range or mkswap manually"
    exit 1
fi

losetup "${loop_dev}" "${SWAP_IMAGE}"
mkswap "${loop_dev}"
losetup -d "${loop_dev}"
