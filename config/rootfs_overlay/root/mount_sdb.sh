#!/bin/sh
#
# 2021-08-18 J. Dagger
#
# Set up the buildroot environment on QEMU for testing the pim-swap module

# add extra drive
echo w | fdisk /dev/sdb
mke2fs /dev/sdb
mount /dev/sdb /media

# now the extra drive is added