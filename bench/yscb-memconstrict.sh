#!/bin/bash
#
# Runs ycsb-c5 with various memory constraints

./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c5.sh 12288
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c5.sh 10240
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c5.sh 8192
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c5.sh 6144
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c5.sh 4096
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c5.sh 16384
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c5.sh 24576
