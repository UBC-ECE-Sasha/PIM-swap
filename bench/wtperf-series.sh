#!/bin/bash

sudo ./limit-mem.sh -m 16384 -d /scratch/ramdisk
./wtperf-test.sh conf/wiredtiger/configs/ycsb-a_8G.wtperf
./wtperf-test.sh conf/wiredtiger/configs/ycsb-a_16G.wtperf
./wtperf-test.sh conf/wiredtiger/configs/ycsb-a_24G.wtperf
./wtperf-test.sh conf/wiredtiger/configs/ycsb-a_32G.wtperf

./wtperf-test.sh conf/wiredtiger/configs/ycsb-c_8G.wtperf
./wtperf-test.sh conf/wiredtiger/configs/ycsb-c_16G.wtperf
./wtperf-test.sh conf/wiredtiger/configs/ycsb-c_24G.wtperf
./wtperf-test.sh conf/wiredtiger/configs/ycsb-c_32G.wtperf
