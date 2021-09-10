#!/bin/bash
#
# Runs ycsb-c4 with various memory constraints

echo group 1 - c4
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 16384
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 14336
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 12288
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 10240
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 9216
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 8192
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 7168
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 6144
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 5120
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 4096
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 3072
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 2048
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 1024

echo group 1 - a4
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 16384
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 14336
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 12288
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 10240
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 9216
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 8192
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 7168
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 6144
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 5120
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 4096
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 3072
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 2048

echo group 1 - a5
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 16384
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 14336
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 12288
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 10240
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 9216
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 8192
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 7168
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 6144
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 5120
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 4096
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 3072
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 2048

echo group 2 - c4
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 16384
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 14336
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 12288
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 10240
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 9216
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 8192
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 7168
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 6144
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 5120
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 4096
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 3072
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 2048
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-c4.sh 1024

echo group 2 - a4
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 16384
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 14336
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 12288
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 10240
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 9216
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 8192
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 7168
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 6144
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 5120
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 4096
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 3072
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a4.sh 2048

echo group 2 - a5
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 16384
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 14336
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 12288
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 10240
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 9216
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 8192
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 7168
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 6144
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 5120
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 4096
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 3072
./qemu-test.sh wiredtiger-copy.sh wiredtiger-run-ycsb-a5.sh 2048
