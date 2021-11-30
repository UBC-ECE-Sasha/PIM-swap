#!/bin/bash

cd ../module and make
cd -

./conf/wiredtiger/WT-make.sh

WTPERF_DIR="./wiredtiger/build_posix/bench/wtperf"
WTPERF_CONFIGS="./wiredtiger/bench/wtperf/runners"

MEMFULL=16384
MEM75=$((3*MEMFULL/4))
MEM50=$((MEMFULL/2))

run_test() {
    M_TARGET=${1:-16384}

    MEM_AVAIL_KB=$(cat /proc/meminfo | grep MemAvailable | awk '{print $2}')
    MEM_AVAIL_MB=$((MEM_AVAIL_KB/1024))
    ./limit-mem.sh -m $M_TARGET -p $MEM_AVAIL_MB -r

    cd $WTPERF_DIR

    LOG_DIR="../../../../logs/WT_YCSB_At_${M_TARGET}_$(date '+%Y-%m-%d--%H-%M-%S')"
    mkdir $LOG_DIR

    ./../../../../log_mem.sh > $LOG_DIR/sys.log 2>&1 &
    MEMLOG_PID=$!

    ./wtperf -O ../../../bench/wtperf/runners/ycsb-at.wtperf

    kill $MEMLOG_PID

    cp WT_TEST/monitor $LOG_DIR
    cp WT_TEST/test.stat $LOG_DIR

    cd -
    ./limit-mem.sh -m $M_TARGET -p $MEM_AVAIL_MB -u
    
    sleep 1
}

cd ../module && make
cd -

for M_TEST in $MEM75
do

    echo 0 > /sys/module/zswap/parameters/enabled
    run_test $M_TEST

    # PIM-swap with 8GB
    cd ../module
    modprobe pimswap.o
    cd -
    run_test $M_TEST

    # zswap with 8GB
    modprobe -r pimswap.o
    echo 1 > /sys/module/zswap/parameters/enabled
    run_test $M_TEST

    # no swap cache + 8GB
    echo 0 > /sys/module/zswap/parameters/enabled
    run_test $(($M_TEST + 8192))
done