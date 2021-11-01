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

    sleep 2
    
    cat /proc/meminfo
}

clear_ramdisk

cd wiredtiger/build_posix/bench/wtperf
mount -t ramfs -o size=16GB ext4 /mnt/ramdisk

for M_TARGET in 4096 14336 # {14336..2048..4096} 
do
    MEM_AVAIL_KB=$(cat /proc/meminfo | grep MemAvailable | awk '{print $2}')
    MEM_AVAIL_MB=$((MEM_AVAIL_KB/1024))

    LOG_DIR="../../../../logs/WT_YCSB_Cx_${M_TARGET}_$(date '+%Y-%m-%d--%H-%M-%S')"
    mkdir $LOG_DIR
    specify_memory $M_TARGET $MEM_AVAIL_MB

    ./../../../../log_mem.sh > $LOG_DIR/sys.log 2>&1 &
    MEMLOG_PID=$!

    ./wtperf -O ../../../bench/wtperf/runners/ycsb-cx.wtperf

    kill $MEMLOG_PID

    cp WT_TEST/monitor $LOG_DIR
    cp WT_TEST/test.stat $LOG_DIR

    clear_ramdisk
    sleep 1
done

for M_TARGET in 4096 14336 # {14336..2048..4096} 
do
    MEM_AVAIL_KB=$(cat /proc/meminfo | grep MemAvailable | awk '{print $2}')
    MEM_AVAIL_MB=$((MEM_AVAIL_KB/1024))

    LOG_DIR="../../../../logs/WT_YCSB_Av_${M_TARGET}_$(date '+%Y-%m-%d--%H-%M-%S')"
    mkdir $LOG_DIR
    specify_memory $M_TARGET $MEM_AVAIL_MB

    ./../../../../log_mem.sh > $LOG_DIR/sys.log 2>&1 &
    MEMLOG_PID=$!

    ./wtperf -O ../../../bench/wtperf/runners/ycsb-av.wtperf

    kill $MEMLOG_PID

    cp WT_TEST/monitor $LOG_DIR
    cp WT_TEST/test.stat $LOG_DIR

    clear_ramdisk
    sleep 1
done

umount /mnt/ramdisk
