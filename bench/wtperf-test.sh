#!/bin/bash
#
# 2022-03-21 J. Dagger (JacksonDDagger at gmail)
#
# Execute wtperf and log extra system statistics.
# Usage: ./wtperf-test.sh [TEST_CONFIG] [MEM_LIMIT]

CONFIG=$1
EXTRA_LOG_NAME=$2
CONFIG_BASENAME="$(basename $CONFIG)"

WTPERF_LOC="wiredtiger/build_posix/bench/wtperf"
DB_DIR=/scratch/db/WT_TEST
LOG_DIR="logs/WT_${CONFIG_BASENAME}_${EXTRA_LOG_NAME}$(date '+%Y-%m-%d--%H-%M-%S')"
mkdir $LOG_DIR
echo $LOG_DIR

$WTPERF_LOC/wtperf -O $CONFIG -h $DB_DIR > $LOG_DIR/wt_out.log 2>&1 &
WTPERF_PID=$!
./process-log.sh $WTPERF_PID $LOG_DIR

cp $DB_DIR/monitor.json $LOG_DIR/monitor.json
cp $DB_DIR/test.stat $LOG_DIR/test.stat

# TODO check pidof wtperf and alert of stray prcesses
kill $WTPERF_PID