#!/bin/bash
#
# 2021-08-11 J. Dagger
#
# Copy built wiredtiger executables over to running QEMU guest
# Will fail if QEMU guest isn't running or blocks connections
# Assumes SSH port of 10022

WT_DIR="wiredtiger"
WTPERF_PATH="${WT_DIR}/build/bench/wtperf/wtperf"

./WT-make.sh

# create directory structure
sshpass -p "root" ssh root@localhost -p 10022 -o "UserKnownHostsFile /dev/null" -o StrictHostKeyChecking=no 'mkdir wiredtiger && mkdir wiredtiger/build_posix'

# copy wiredtiger/wtperf executables and configurations over
sshpass -p "root" scp -o "UserKnownHostsFile /dev/null" -o StrictHostKeyChecking=no -P 10022 -r ${WT_DIR}/bench/ root@localhost:wiredtiger/
sshpass -p "root" scp -o "UserKnownHostsFile /dev/null" -o StrictHostKeyChecking=no -P 10022 -r ${WT_DIR}/build_posix/.libs/ root@localhost:wiredtiger/build_posix/
sshpass -p "root" scp -o "UserKnownHostsFile /dev/null" -o StrictHostKeyChecking=no -P 10022 -r ${WT_DIR}/build_posix/bench/ root@localhost:wiredtiger/build_posix/
