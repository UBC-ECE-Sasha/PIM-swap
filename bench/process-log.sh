#!/bin/bash
#
# 2022-03-21 J. Dagger (JacksonDDagger at gmail)
#
# Log the system and given process with pidstat, vmstat and perf.
# Automatically terminates when given PID terminates.
# Usage: ./process-log.sh [PID] [LOG_DIR]

if [ "$1" == "-h" ]; then
  echo "Usage: ./process-log.sh [PID] [LOG_DIR]"
  exit 0
fi

PID=$1
LOG_DIR=$2

SYS_STATE_LOG=${LOG_DIR}/start.log
echo "System state at start of log: $(date)" > ${SYS_STATE_LOG}
echo "/proc/meminfo" >> ${SYS_STATE_LOG}
cat /proc/meminfo >> ${SYS_STATE_LOG}
echo "/proc/cpuinfo" >> ${SYS_STATE_LOG}
cat /proc/cpuinfo >> ${SYS_STATE_LOG}
echo "zswap" >> ${SYS_STATE_LOG}
grep -R . /sys/module/zswap/parameters >> ${SYS_STATE_LOG}


vmstat -n -t 1 > $LOG_DIR/vmstat.log 2>&1 &
MEMLOG_PID=$!
./log-pages.sh > $LOG_DIR/pages.csv 2>&1 &
LOG_PID=$!
perf record -e major-faults -e minor-faults -o $LOG_DIR/perf.data > $LOG_DIR/perf.log 2>&1 &
pidstat -p $PID -r -u 1 > $LOG_DIR/pidstat.log 2>&1
kill $MEMLOG_PID
kill $LOG_PID
