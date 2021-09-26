#!/bin/bash
#
# Runs ycsb-c4 with various memory constraints

CONF_DIR="conf/wiredtiger"
HOST_FILE="${CONF_DIR}/wiredtiger-copy.sh"
GUEST_FILE="${CONF_DIR}/wiredtiger-run-ycsb-c4.sh"
CORES=4
DISK_IMG="/media/jackson/WDC_HDD/disk_imgs/disk_120G.raw" 

for MEM in {2048..16384..2048}
do
    echo testing $MEM
    ./qemu-test.sh -i $HOST_FILE -g $GUEST_FILE -m $MEM -c $CORES -d $DISK_IMG -e
done
