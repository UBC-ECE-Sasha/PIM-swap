#!/bin/bash

CONFIG=$1
EXTRA_LOG_NAME=$2
CONFIG_BASENAME="$(basename $CONFIG)"

WTPERF_LOC="wiredtiger/build_posix/bench/wtperf"
DB_DIR=/scratch/db/WT_TEST
LOG_DIR="logs/WT_${CONFIG_BASENAME}_${EXTRA_LOG_NAME}$(date '+%Y-%m-%d--%H-%M-%S')"
mkdir $LOG_DIR
echo $LOG_DIR

./log-mem.sh > $LOG_DIR/sys.log 2>&1 &
MEMLOG_PID=$!

$WTPERF_LOC/wtperf -O $CONFIG -h $DB_DIR > $LOG_DIR/wt_out.log 2>&1 &
WTPERF_PID=$!

pidstat -p $WTPERF_PID -r -u 1 > $LOG_DIR/pidstat.log 2>&1
kill $MEMLOG_PID

cp $DB_DIR/monitor.json $LOG_DIR/monitor.json
cp $DB_DIR/test.stat $LOG_DIR/test.stat

# TODO check pidof wtperf and alert of stray prcesses
kill $WTPERF_PID