#!/bin/bash

CONFIG=$1
CONFIG_BASENAME="$(basename $CONFIG)"

WTPERF_LOC="wiredtiger/build_posix/bench/wtperf"
LOG_DIR="logs/WT_${CONFIG_BASENAME}_$(date '+%Y-%m-%d--%H-%M-%S')"
mkdir $LOG_DIR

./log_mem.sh > $LOG_DIR/sys.log 2>&1 &
MEMLOG_PID=$!

$WTPERF_LOC/wtperf -O $CONFIG > $LOG_DIR/wt_out.log 2>&1 &
WTPERF_PID=$!

pidstat -p $WTPERF_PID -r -u 1 > $LOG_DIR/pidstat.log 2>&1
kill $MEMLOG_PID