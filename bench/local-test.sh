#!/bin/bash
#
# 2022-03-21 J. Dagger (JacksonDDagger at gmail)
#
# Benchmark workload on local machine with memory limits.

# command line options defaults

RAMFS_DIR=/scratch/ramdisk
MEM_LIMIT="0" # memory limit of test
TIMEOUT="0" # in seconds
CMD_UMOUNT="false"
ZSWAP="false"
TEST_CONFIG="none"
RAMDISK_SIZE="0"

print_usage() {
  echo "Run a workload locally with memory limits."
  echo "Usage: ./local-test.sh [OPTIONS]"
  echo "-c: test config file"
  echo "-d: ramfs directory"
  echo "-m: specify memory limit in MB (0 for no limit)"
  echo "-t: specify timeout in s"
  echo "-u: unmount ramdisk after test"
  echo "-z: enable zswap"
  echo "-h: print this message"
  echo "read more at https://wiki.ubc.ca/PIM-SWAP"
}

while getopts 'c:d:m:t:uzh' flag; do
  case "${flag}" in
    c) TEST_CONFIG="${OPTARG}" ;;
    d) RAMFS_DIR="${OPTARG}" ;;
    m) MEM_LIMIT="${OPTARG}" ;;
    t) TIMEOUT="${OPTARG}" ;;
    u) CMD_UMOUNT="true" ;;
    z) ZSWAP="true" ;;
    h) print_usage
       exit 0 ;;
    *) print_usage
       exit 1 ;;
  esac
done

if [ $MEM_LIMIT -gt 0 ]; then  
  ./limit-mem.sh -d $RAMFS_DIR -m $MEM_LIMIT
fi

if [ "$ZSWAP" == "true" ]; then
  #sudo echo 1 > /sys/module/zswap/parameters/enabled
  LOG_EXTRA=ZSWAP_
#else
  #sudo echo 0 > /sys/module/zswap/parameters/enabled
fi

LOG_EXTRA+=${MEM_LIMIT}M_

# TODO check name of config 
./wtperf-test.sh $TEST_CONFIG $LOG_EXTRA
if [ $CMD_UMOUNT == "true" ]; then
  ./limit-mem.sh -u -d $RAMFS_DIR
fi