#!/bin/bash
#
# Login with
#user: root
#password: root
#
# Load the module
# modprobe pim_swap

ROOT=buildroot

qemu-system-x86_64 -enable-kvm \
 -kernel output/images/bzImage \
 -initrd output/images/rootfs.cpio.gz \
 -append "console=ttyS0" \
 -cpu host \
 -m 2248 \
 -usb \
 -chardev pty,id=ser0 \
 -serial chardev:ser0 \
 -net user,hostfwd=tcp::10022-:22 -net nic \
 -nographic \
 -drive file=swap-1g.raw,format=raw,if=ide \
 -drive id=additional_disk,file=disk_5G.raw,if=ide,format=raw 
# For VNC: -vnc :1

# user networking, forward host port to ssh quest port
# Setup a basic virtual NIC

# Bridge:
# -netdev tap,id=hostnet0,script=qemu-ifup -device e1000,netdev=hostnet0,mac=52:54:00:70:90:b0
