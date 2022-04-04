#!/bin/bash
#
# 2022-03-21 J. Dagger (JacksonDDagger at gmail)
#
# Log the system and given process with pidstat, vmstat and perf.
# Automatically terminates when given PID terminates.

PID=$1
LOG_DIR=$2

vmstat -n -t 1 > $LOG_DIR/vmstat.log 2>&1 &
MEMLOG_PID=$!
perf record -e major-faults -e minor-faults -o $LOG_DIR/perf.data > $LOG_DIR/perf.log 2>&1 &
pidstat -p $PID -r -u 1 > $LOG_DIR/pidstat.log 2>&1
kill $MEMLOG_PID
