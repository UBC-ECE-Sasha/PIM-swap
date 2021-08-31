#!/bin/bash
#
# Runs ycsb-c4 with various memory constraints

./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 24576
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 16384
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 12288
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 10240
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 8192
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 6144
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 4096
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 3072
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 2048
