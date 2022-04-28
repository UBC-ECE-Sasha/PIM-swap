#!/bin/bash

./local-test.sh -c conf/wiredtiger/configs/ycsb-c_16G.wtperf
./local-test.sh -m 49152 -c conf/wiredtiger/configs/ycsb-c_16G.wtperf
./local-test.sh -m 49152 -z -c conf/wiredtiger/configs/ycsb-c_16G.wtperf
./local-test.sh -m 32768 -c conf/wiredtiger/configs/ycsb-c_16G.wtperf
./local-test.sh -m 32768 -z -c conf/wiredtiger/configs/ycsb-c_16G.wtperf
./local-test.sh -m 16384 -c conf/wiredtiger/configs/ycsb-c_16G.wtperf
./local-test.sh -m 16384 -z -c conf/wiredtiger/configs/ycsb-c_16G.wtperf
