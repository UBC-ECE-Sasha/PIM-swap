#!/bin/bash
#
# 2022-04-27 J. Dagger (JacksonDDagger at gmail)
#
# Get active and inactive pages from proc meminfo

INTERVAL=1
N_LOOPS=0

if [ "$#" -gt 2 ]; then
    echo "Wrong."
elif [ "$#" -eq 2 ]; then
    INTERVAL=$1
    N_LOOPS=$2
elif [ "$#" -eq 1 ]; then
    INTERVAL=$1
fi

print_line () {
    DATE=$(date +%F" "%T)
    ACTIVE_A=$(grep -m 1 "Active(anon)" /proc/meminfo | awk '{ print $2 }')
    INACTIVE_A=$(grep -m 1 "Inactive(anon)" /proc/meminfo | awk '{ print $2 }')
    ACTIVE_F=$(grep -m 1 "Active(file)" /proc/meminfo | awk '{ print $2 }')
    INACTIVE_F=$(grep -m 1 "Inactive(file)" /proc/meminfo | awk '{ print $2 }')
    echo "${DATE}, ${ACTIVE_A}, ${INACTIVE_A}, ${ACTIVE_F}, ${INACTIVE_F}"
    sleep $INTERVAL
}

echo "date, Active(anon), Inactive(anon), Active(file), Inactive(file)"

if [ "$N_LOOPS" -gt 0 ]; then
    for I in 'seq 0 $N_LOOPS'
    do
        print_line
    done
else
    while true
    do
        print_line
    done
fi

