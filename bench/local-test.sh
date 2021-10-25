#!/bin/bash
clear_ramdisk () {
    rm -f /mnt/ramdisk/testfile
}

# pass in MB to fill ramdisk testfile with
fill_ramdisk () {
    dd < /dev/zero bs=1048576 count=$1 > /mnt/ramdisk/testfile
}

specify_memory () {
    TARGET_AVAIL=$1
    PREV_MEM_AVAIL=$2

    FILL_MB=$((PREV_MEM_AVAIL-TARGET_AVAIL))
    echo fill $FILL_MB
    fill_ramdisk $FILL_MB

    cat /proc/meminfo
}

cd wiredtiger/build_posix/bench/wtperf

for M_TARGET in {2048..14336..2048}
do
    MEM_AVAIL_KB=$(cat /proc/meminfo | grep MemAvailable | awk '{print $2}')
    MEM_AVAIL_MB=$((MEM_AVAIL_KB/1024))

    mount -t ramfs -o size=16GB ext4 /mnt/ramdisk
    chmod 777 /mnt/ramdisk/
    LOG_DIR="../../../../logs/WT_YCSB_Ax_${M_TARGET}_$(date '+%Y-%m-%d--%H-%M-%S')"
    mkdir $LOG_DIR
    specify_memory $M_TARGET $MEM_AVAIL_MB

    ./wtperf -O ../../../bench/wtperf/runners/ycsb-ax.wtperf

    cp WT_TEST/monitor $LOG_DIR
    cp WT_TEST/test.stat $LOG_DIR

    clear_ramdisk
    umount -f -v /mnt/ramdisk
    sleep 1
    wait
done

umount /mnt/ramdisk
