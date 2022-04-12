#!/bin/bash
#
# 2021-11-30 J. Dagger (JacksonDDagger at gmail)
# 
# Limit memory on system using ramfs.
# Script assumes ramfs is already mounted unless -r is specified.

RAMFS_DIR=/mnt/ramdisk
RAMFS_NUM=4
RAMFS_DENOM=5
RAMFS_SIZE=$((RAMFS_NUM*$(cat /proc/meminfo | grep MemTotal | awk '{print $2}')/RAMFS_DENOM/1024/1024))
MEMLEFT=0
PREV_MEM_AVAIL=4096
CMD_MOUNT="true"
CMD_UNMOUNT="false"
CMD_CLEAR="false"

print_usage() {
  echo "-d: ramfs directory"
  echo "-m: specify memory limit in MB"
  echo "-s: specify size of ramfs in MB (default will be slightly bigger than needed)"
  echo "-c: clear ramfs"
  echo "-r: don't mount ramdisk"
  echo "-u: unmount ramdisk"
  echo "-h: print this message"
  echo "read more at https://wiki.ubc.ca/PIM-SWAP"
}

while getopts 'd:m:p:s:cruh' flag; do
  case "${flag}" in
    d) RAMFS_DIR="${OPTARG}" ;;
    m) MEMLEFT="${OPTARG}" ;;
    s) RAMFS_SIZE="${OPTARG}" ;;
    c) CMD_CLEAR="true" ;;
    r) CMD_MOUNT="false" ;;
    u) CMD_UNMOUNT="true" ;;
    h) print_usage
       exit 1 ;;
    *) print_usage
       exit 1 ;;
  esac
done

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
        cat /proc/meminfo
    fi
}

mount_ramdisk () {
    mount -t ramfs -o size=${RAMFS_SIZE}G ext4 ${RAMFS_DIR}
}

umount_ramdisk () {
    umount ${RAMFS_DIR}
}

if [ $CMD_CLEAR == "true" ]; then
    clear_ramdisk
fi

if [ $CMD_UNMOUNT == "true" ]; then
    umount_ramdisk

    if [ "$CMD_MOUNT" != "true" ]; then
        MEMLEFT=0
    fi
fi

if [ $CMD_MOUNT == "true" ]; then
    mount_ramdisk
fi

specify_memory
