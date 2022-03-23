#!/bin/bash
#
# 2021-11-30 J. Dagger (JacksonDDagger at gmail)
#
# Setup fresh server for PIM-swap benchmarking.

sudo apt-get install sysstat bc

mkdir /scratch/db
chmod 777 /scratch/db
mkdir /scratch/ramdisk
chmod 777 /scratch/ramdisk