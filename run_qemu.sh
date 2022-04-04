#!/bin/bash
#
# Login with
#user: root
#password: root
#
# Load the module
# modprobe pim_swap

ROOT=buildroot
RAM_SIZE=100
id=0
NR_RANKS=1
NR_SLOTS=2
MAX_MEM=0
MAPPING_OPTIONS=""

id=0                                                                             
while [ $id -lt $NR_RANKS ]; do 
    if [ ! -c "/dev/dax$id.$id" ]; then
        die "Unable to map $NR_RANKS ranks"
    fi
    # Add 200M per rank
    RAM_SIZE=$((RAM_SIZE + 200))
    MAPPING_OPTIONS="$MAPPING_OPTIONS --object memory-backend-file,id=pim$id,size=8G,mem-path=/dev/dax$id.$id,align=1G --device pc-dimm,id=dimm$id,memdev=pim$id"
    id=$((id + 1))
done

MAX_MEM=$((RAM_SIZE + 8192*NR_RANKS))
 
qemu-system-x86_64 -s -S -M q35 -enable-kvm \
 -m ${RAM_SIZE}M,slots=$NR_SLOTS,maxmem=${MAX_MEM}M $MAPPING_OPTIONS \
 -kernel  output/build/linux-5.4.95/arch/x86_64/boot/bzImage \
 -append "rootwait root=/dev/vda console=ttyS0 nokaslr memmap=2G!4G" \
 -initrd output/images/rootfs.cpio.gz \
 -cpu host \
 -usb \
 -chardev pty,id=ser0 \
 -serial chardev:ser0 \
 -net user,hostfwd=tcp::10022-:22 -net nic \
 -nographic \
 -drive file=swap-1g.raw,if=ide,format=raw 
# For VNC: -vnc :1

# user networking, forward host port to ssh quest port
# Setup a basic virtual NIC

# Bridge:
# -netdev tap,id=hostnet0,script=qemu-ifup -device e1000,netdev=hostnet0,mac=52:54:00:70:90:b0
