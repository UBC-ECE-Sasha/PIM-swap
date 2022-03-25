#!/bin/bash

PID=$1
LOG_DIR=$2

vmstat -n -t 1 > $LOG_DIR/vmstat.log 2>&1 &
MEMLOG_PID=$!
perf record -e page-faults -o $LOG_DIR/perf.data > $LOG_DIR/perf.log 2>&1 &
pidstat -p $PID -r -u 1 > $LOG_DIR/pidstat.log 2>&1
kill $MEMLOG_PID
