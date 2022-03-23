#!/bin/bash

# command line options defaults
MEM_LIMIT="4096" # memory limit of test
TIMEOUT="0" # in seconds
TEST_CONFIG="none"
RAMDISK_SIZE="24576"

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

MEM_AVAIL_KB=$(cat /proc/meminfo | grep MemAvailable | awk '{print $2}')
MEM_AVAIL_MB=$((MEM_AVAIL_KB/1024))

./limit-mem.sh -d /scratch/ramdisk -m $MEM_LIMIT

# TODO check name of config 
./wtperf-test.sh $TEST_CONFIG

clear_ramdisk


