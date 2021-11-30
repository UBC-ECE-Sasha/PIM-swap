#!/bin/bash
#
# 2021-08-11 J. Dagger
#
# Copy built wiredtiger executables over to running QEMU guest
# Will fail if QEMU guest isn't running or blocks connections
# Assumes SSH port of 10022

TOP_DIR="."
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
    cd wiredtiger
    ./build_posix/reconf
    cd ./build_posix
    ../configure 
    make
    cd ../../
    echo "wtperf built"
fi

cp -a $TOP_DIR/conf/wiredtiger/configs/. $WT_DIR/bench/wtperf/runners