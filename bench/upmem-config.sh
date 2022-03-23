#!/bin/bash

WORK_DIR=/scratch/work
mkdir -p "$WORK_DIR"

# cd ../module && make
# cd -

# ./conf/wiredtiger/WT-make.sh

WT_SRC_DIR="wiredtiger"
WT_DIR="${WORK_DIR}/wiredtiger"
WTPERF_DIR="${WT_DIR}/build_posix/bench/wtperf"
WTPERF_CONFIGS="./wiredtiger/bench/wtperf/runners"

mkdir -p "$WT_DIR"
mkdir -p "$WT_DIR/build_posix"
cp -R ${WT_SRC_DIR}/build_posix/.libs/ ${WT_DIR}/build_posix/
cp -R ${WT_SRC_DIR}/build_posix/bench/ ${WT_DIR}/build_posix/
cp -R ${WT_SRC_DIR}/bench/ ${WT_DIR}/

BENCH_DIR=$(pwd)

MEMFULL=16384
MEM75=$((3*MEMFULL/4))
MEM50=$((MEMFULL/2))

run_test() {
    M_TARGET=${1:-63488}

    MEM_AVAIL_KB=$(cat /proc/meminfo | grep MemAvailable | awk '{print $2}')
    MEM_AVAIL_MB=$((MEM_AVAIL_KB/1024))
    ./limit-mem.sh -m $M_TARGET -p $MEM_AVAIL_MB -r

    cd $WTPERF_DIR

    #LOG_DIR="../../../../logs/WT_YCSB_At_${M_TARGET}_$(date '+%Y-%m-%d--%H-%M-%S')"
    LOG_HOME="$BENCH_DIR/logs"
    mkdir -p logs

    mkdir -p "../../../../logs"
    LOG_DIR="../../../../logs/WT_YCSB_C_10m_16G_$(date '+%Y-%m-%d--%H-%M-%S')"
    mkdir $LOG_DIR

    ./$BENCH_DIR/log-mem.sh > $LOG_DIR/sys.log 2>&1 &
    MEMLOG_PID=$!

    ./wtperf -O ../../../bench/wtperf/runners/ycsb-c_10m_16G.wtperf

    kill $MEMLOG_PID

    cp WT_TEST/monitor $LOG_DIR
    cp WT_TEST/test.stat $LOG_DIR

    cp $LOG_DIR $LOG_HOME

    cd -
    ./limit-mem.sh -u
    
    sleep 1
}

run_test
exit

for M_TEST in 60
do

    echo 0 > /sys/module/zswap/parameters/enabled
    run_test $M_TEST

    # PIM-swap with 8GB
    cd ../module
    modprobe pimswap.o
    cd -
    run_test $M_TEST

    # zswap with 8GB
    modprobe -r pimswap.ko
    echo 1 > /sys/module/zswap/parameters/enabled
    run_test $M_TEST

    # no swap cache + 8GB
    echo 0 > /sys/module/zswap/parameters/enabled
    run_test $(($M_TEST + 8192))
done