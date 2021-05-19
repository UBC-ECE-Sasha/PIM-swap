#!/bin/bash
#
# 2020-02-10 J.Nider
#
# Set up the buildroot environment on QEMU for testing the pim-swap module

# load the module
modprobe pim_swap

# set up swapping, assuming fstab already has /dev/sda as a swap device
swapon -a

# now you are ready to run stress-ng or some other test
