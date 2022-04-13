#!/bin/bash
#
# 2022-03-21 J. Dagger (JacksonDDagger at gmail)
#
# Benchmark workload on local machine with memory limits.

# command line options defaults
MEM_LIMIT="0" # memory limit of test
TIMEOUT="0" # in seconds
TEST_CONFIG="none"
RAMDISK_SIZE="0"

print_usage() {
  echo "-c: test config file"
  echo "-m: specify memory limit in MB (0 for no limit)"
  echo "-t: specify size of ramfs in MB (default will be slightly bigger than needed)"
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

if [ $MEM_LIMIT -gt 0 ]; then  
  if [ $RAMDISK_SIZE -gt 0]; then
    ./limit-mem.sh -r -d /scratch/ramdisk -m $MEM_LIMIT -s $RAMDISK_SIZE
  else
    ./limit-mem.sh -r -d /scratch/ramdisk -m $MEM_LIMIT
  fi
fi

# TODO check name of config 
./wtperf-test.sh $TEST_CONFIG ${MEM_LIMIT}M_
./limit-mem.sh -u -d /scratch/ramdisk