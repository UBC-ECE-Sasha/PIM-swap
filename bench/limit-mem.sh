#!/bin/bash
#
# 2021-11-30 J. Dagger (JacksonDDagger at gmail)
# 
# Limit memory on system using ramfs.
# Script assumes ramfs is already mounted unless -r is specified.

RAMFS_DIR=/mnt/ramdisk
RAMFS_SIZE=0
MEMLEFT=0
PREV_MEM_AVAIL=4096
CMD_UNMOUNT="false"
CMD_CLEAR="false"

print_usage() {
  echo "Limit memory on system using ramfs."
  echo "-d: ramfs directory"
  echo "-m: specify memory limit in MB"
  echo "-s: specify size of ramfs in MB (default will be slightly bigger than needed)"
  echo "-c: clear ramfs"
  echo "-u: unmount ramdisk"
  echo "-h: print this message"
  echo "read more at https://wiki.ubc.ca/PIM-SWAP"
}

while getopts 'd:m:p:s:cuh' flag; do
  case "${flag}" in
    d) RAMFS_DIR="${OPTARG}" ;;
    m) MEMLEFT="${OPTARG}" ;;
    s) RAMFS_SIZE="${OPTARG}" ;;
    c) CMD_CLEAR="true" ;;
    u) CMD_UNMOUNT="true" ;;
    h) print_usage
       exit 1 ;;
    *) print_usage
       exit 1 ;;
  esac
done

TOTAL_MEM_MB=$(($(cat /proc/meminfo | grep MemTotal | awk '{print $2}')/1024))
if [ "$TOTAL_MEM_MB" -lt "$MEMLEFT" ]; then
  echo "Error: Memory limit is greater than total memory."
  exit 1
fi

if [ "$RAMFS_SIZE" -le 0 ]; then
    if [ "$TOTAL_MEM_MB" -gt 4096 ]; then
        RAMFS_SIZE=$((TOTAL_MEM_MB - 2048))
    else
        RAMFS_NUM=3
        RAMFS_DENOM=4
        RAMFS_SIZE=$((RAMFS_NUM*TOTAL_MEM_MB/RAMFS_DENOM))
    fi
fi

TESTFILE=${RAMFS_DIR}/testfile
MEM_AVAIL_KB=$(cat /proc/meminfo | grep MemAvailable | awk '{print $2}')
MEM_AVAIL_MB=$((MEM_AVAIL_KB/1024))

clear_ramdisk () {
    rm -f ${TESTFILE}
}

# pass in MB to fill ramdisk testfile with
fill_ramdisk () {
    dd < /dev/zero bs=1048576 count=$1 >> ${TESTFILE}
}

specify_memory () {
    if [ $MEMLEFT -gt 0 ]; then  
        clear_ramdisk

        FILL_MB=$((MEM_AVAIL_MB-MEMLEFT))
        if [ "$FILL_MB" -gt "0" ]; then
            echo fill $FILL_MB
            fill_ramdisk $FILL_MB
            sleep 2
        fi
    fi
}

mount_ramdisk () {
    sudo mount -t ramfs -o size=${RAMFS_SIZE}M ext4 ${RAMFS_DIR}
    sudo chmod 777 ${RAMFS_DIR}
}

umount_ramdisk () {
    umount ${RAMFS_DIR}
}

if [ $CMD_CLEAR == "true" ]; then
    clear_ramdisk
fi

if [ $CMD_UNMOUNT == "true" ]; then
    umount_ramdisk
fi

FOUND_RAMFS_DIR=$(mount | grep ramfs | awk '{print $3}')
if [ -z "$FOUND_RAMFS_DIR" ]; then
    mount_ramdisk
elif [ "$FOUND_RAMFS_DIR" != *"$RAMFS_DIR"* ]; then
    mount_ramdisk
fi

specify_memory
