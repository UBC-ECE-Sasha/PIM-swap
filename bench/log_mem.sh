#!/bin/bash
#
# 2021-09-29 J. Dagger
#
# Output memory usage statistics approximately twice a second

PROC="${1:-wtperf}"
echo "date,free mem(kB),available mem(kB),buffers(kB),cached(kB),free swap(kB),cached swap(kB),$PROC virtmem(kB),$PROC phymem(kB),$PROC CPU(%)"

print_line(){
    FREE_MEM=$(awk '/^MemFree/{print $(NF-1)}' /proc/meminfo)
    AVAILABLE_MEM=$(awk '/^MemAvailable/{print $(NF-1)}' /proc/meminfo)
    BUFFERS=$(awk '/^Buffers/{print $(NF-1)}' /proc/meminfo)
    CACHED=$(awk '/^Cached/{print $(NF-1)}' /proc/meminfo)
    FREE_SWAP=$(awk '/^SwapFree/{print $(NF-1)}' /proc/meminfo)
    CACHED_SWAP=$(awk '/^SwapCached/{print $(NF-1)}' /proc/meminfo)
    PROC_VIRT=$(top -n 1 -d 1 -b | awk -v X=$PROC '($12==X) {print $5}')
    PROC_RES=$(top -n 1 -d 1 -b | awk -v X=$PROC '($12==X) {print $6}')
    PROC_CPU=$(top -n 1 -d 1 -b | awk -v X=$PROC '($12==X) {print $9}')

    if [[ $PROC_VIRT == *"g" ]]; then
    PROC_VIRT="${PROC_VIRT//g}"
    PROC_VIRT=$(echo "$PROC_VIRT*1024*1024 / 1" | bc)
    fi

    if [[ $PROC_RES == *"g" ]]; then
    PROC_RES="${PROC_RES//g}"
    PROC_RES=$(echo "$PROC_RES*1024*1024 / 1" | bc)
    fi

    # default is 0
    #: "${PROC_VIRT:=0}"
    #: "${PROC_RES:=0}"

    echo "$(date +"%Y/%m/%d-%T"),$FREE_MEM,$AVAILABLE_MEM,$BUFFERS,$CACHED,$FREE_SWAP,$CACHED_SWAP,$PROC_VIRT,$PROC_RES,$PROC_CPU"
}

while :
do
    print_line
    sleep 2
done
