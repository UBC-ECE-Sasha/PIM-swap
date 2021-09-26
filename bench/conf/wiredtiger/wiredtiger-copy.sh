#!/bin/bash
#
# 2021-08-11 J. Dagger
#
# Copy built wiredtiger executables over to running QEMU guest
# Will fail if QEMU guest isn't running or blocks connections
# Assumes SSH port of 10022

WT_DIR="wiredtiger"
WTPERF_PATH="${WT_DIR}/build_posix/bench/wtperf/wtperf"

# clone wiredtiger if it doesn't exist
if [ -d "wiredtiger" ]; then
    echo "Using files in ${WT_DIR}"
else
    echo "Cloning wiredtiger into ${WT_DIR}"
    git clone https://github.com/wiredtiger/wiredtiger.git -b develop
fi

# build wiredtiger if it isn't built
if [ -f "$WTPERF_PATH" ]; then
    echo "Using wtperf in ${PWD}/${WTPERF_PATH}"
else
    echo "wtperf not found. Building in ${WT_DIR}"

    echo "wtperf built"
fi

cp -a configs/wtperf/. $WT_DIR/bench/wtperf/runners

# create directory structure
sshpass -p "root" ssh root@localhost -p 10022 -o "UserKnownHostsFile /dev/null" -o StrictHostKeyChecking=no 'mkdir wiredtiger && mkdir wiredtiger/build_posix'

# copy wiredtiger/wtperf executables and configurations over
sshpass -p "root" scp -o "UserKnownHostsFile /dev/null" -o StrictHostKeyChecking=no -P 10022 -r ${WT_DIR}/bench/ root@localhost:wiredtiger/
sshpass -p "root" scp -o "UserKnownHostsFile /dev/null" -o StrictHostKeyChecking=no -P 10022 -r ${WT_DIR}/build_posix/.libs/ root@localhost:wiredtiger/build_posix/
sshpass -p "root" scp -o "UserKnownHostsFile /dev/null" -o StrictHostKeyChecking=no -P 10022 -r ${WT_DIR}/build_posix/bench/ root@localhost:wiredtiger/build_posix/
