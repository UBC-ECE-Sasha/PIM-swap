#!/bin/bash

./local-test.sh -m 49152 -c conf/wiredtiger/configs/ycsb-c_24G.wtperf
./local-test.sh -m 24576 -c conf/wiredtiger/configs/ycsb-c_24G.wtperf
./local-test.sh -m 20480 -c conf/wiredtiger/configs/ycsb-c_24G.wtperf
./local-test.sh -m 40960 -c conf/wiredtiger/configs/ycsb-c_24G.wtperf
./local-test.sh -m 36864 -c conf/wiredtiger/configs/ycsb-c_24G.wtperf
./local-test.sh -m 32768 -c conf/wiredtiger/configs/ycsb-c_24G.wtperf
./local-test.sh -m 28672 -c conf/wiredtiger/configs/ycsb-c_24G.wtperf
