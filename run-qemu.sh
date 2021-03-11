#!/bin/bash

ROOT=buildroot

qemu-system-x86_64 -enable-kvm \
 -kernel output/images/bzImage \
 -initrd output/images/rootfs.cpio.gz \
 -append "console=ttyS0" \
 -cpu host \
 -m 1272 \
 -usb \
 -drive file=swap-1g.qcow2,format=qcow2,if=ide \
 -netdev tap,id=hostnet0,script=qemu-ifup -device e1000,netdev=hostnet0,mac=52:54:00:70:90:b0
