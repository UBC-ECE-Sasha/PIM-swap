#!/bin/bash
#
# 2022-03-23 J. Dagger (JacksonDDagger at gmail)
#
# Setup fresh server for PIM-swap benchmarking.

sudo apt-get install sysstat

mkdir /scratch/db
chmod 777 /scratch/db
mkdir /scratch/ramdisk
chmod 777 /scratch/ramdisk