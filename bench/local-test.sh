#!/bin/bash

# command line options defaults
MEM_LIMIT="4096" # memory limit of test
TIMEOUT="0" # in seconds
TEST_CONFIG="none"
RAMDISK_SIZE="24576"

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
    if [ "$FILL_MB" -gt "0" ]; then
        echo fill $FILL_MB
        fill_ramdisk $FILL_MB
        sleep 2
    fi
    
    cat /proc/meminfo
}

print_usage() {
  echo "usage: $0 [-i pathtohostfile] [-g pathtoguestfile] [-m memorymb] [-c simulatedcores] [-d pathtodiskimg] [-t timeout(s)] [-e] [-k]"
  echo "-c config file: wtperf config to use for tests"
  echo "-m memorymb: memory in MB left for program (default: ${MEM_LIMIT})"
  echo "-t timeout(s): test timeout in seconds with 0 for no timeout (default: ${TIMEOUT})"
  echo "-h: print this message"
  echo "read more at https://wiki.ubc.ca/PIM-SWAP"
}

while getopts 'c:m:t:h' flag; do
  case "${flag}" in
    c) TEST_CONFIG="${OPTARG}" ;;
    m) MEM_LIMIT="${OPTARG}" ;;
    t) TIMEOUT="${OPTARG}" ;;
    h) print_usage
       exit 1 ;;
    *) print_usage
       exit 1 ;;
  esac
done

clear_ramdisk

cd wiredtiger/build_posix/bench/wtperf
mount -t ramfs -o size=${RAMDISK_SIZE}MB ext4 /mnt/ramdisk

MEM_AVAIL_KB=$(cat /proc/meminfo | grep MemAvailable | awk '{print $2}')
MEM_AVAIL_MB=$((MEM_AVAIL_KB/1024))

specify_memory $MEM_LIMIT $MEM_AVAIL_MB

./wtperf-test.sh $TEST_CONFIG

cp WT_TEST/monitor $LOG_DIR
cp WT_TEST/test.stat $LOG_DIR

clear_ramdisk


