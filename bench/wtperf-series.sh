#!/bin/bash

sudo ./limit-mem.sh 32768
./wtperf-test.sh conf/wiredtiger/configs/ycsb-a_32G.wtperf
./wtperf-test.sh conf/wiredtiger/configs/ycsb-a_30G.wtperf
./wtperf-test.sh conf/wiredtiger/configs/ycsb-a_28G.wtperf
./wtperf-test.sh conf/wiredtiger/configs/ycsb-a_26G.wtperf
./wtperf-test.sh conf/wiredtiger/configs/ycsb-a_24G.wtperf
