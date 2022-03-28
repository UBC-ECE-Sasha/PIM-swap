#!/bin/bash

sudo ./limit-mem.sh -m 8192 -d /scratch/ramdisk
./wtperf-test.sh conf/wiredtiger/configs/ycsb-a_22G.wtperf
./wtperf-test.sh conf/wiredtiger/configs/ycsb-a_14G.wtperf
./wtperf-test.sh conf/wiredtiger/configs/ycsb-a_16G.wtperf
./wtperf-test.sh conf/wiredtiger/configs/ycsb-a_18G.wtperf
./wtperf-test.sh conf/wiredtiger/configs/ycsb-a_20G.wtperf
./wtperf-test.sh conf/wiredtiger/configs/ycsb-a_22G.wtperf
