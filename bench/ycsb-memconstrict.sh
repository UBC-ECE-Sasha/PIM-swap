#!/bin/bash

CONF_DIR="conf/wiredtiger"
HOST_FILE="${CONF_DIR}/wiredtiger-copy.sh"
GUEST_FILE="${CONF_DIR}/wiredtiger-run-ycsb-ca.sh"
CORES=4
DISK_IMG="/mnt/wd_hdd_350g/disk_160G.raw" 
SWP="/mnt/wd_hdd_350g/swap-32g.raw"

MEM=24576

for i in {1:8}
do
    echo testing $MEM
    ./qemu-test.sh -f ycsb-ca.wtperf -m $MEM -c $CORES -d $DISK_IMG -s $SWP -e
done
