#!/bin/bash
#
# 2021-09-29 J. Dagger
#
# Output memory usage statistics approximately twice a second

PROC="${1:-wtperf}"
echo "date,free mem(kB),available mem(kB),buffers(kB),cached(kB),free swap(kB),cached swap(kB),$PROC virtmem(kB),$PROC phymem(kB),$PROC CPU(%)"

convert_kb(){
    VAL=$1
    if [[ $VAL == *"m" ]]; then
    VAL="${VAL//m}"
    VAL=$(echo "$VAL*1024 / 1" | bc)
    fi
    if [[ $VAL == *"g" ]]; then
    VAL="${VAL//g}"
    VAL=$(echo "$VAL*1024*1024 / 1" | bc)
    fi
    if [[ $VAL == *"t" ]]; then
    VAL="${VAL//t}"
    VAL=$(echo "$VAL*1024*1024*1024 / 1" | bc)
    fi
    echo $VAL
}


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

    PROC_VIRT=$(convert_kb $PROC_VIRT)
    PROC_VIRT=$(convert_kb $PROC_RES)

    echo "$(date +"%Y/%m/%d-%T"),$FREE_MEM,$AVAILABLE_MEM,$BUFFERS,$CACHED,$FREE_SWAP,$CACHED_SWAP,$PROC_VIRT,$PROC_RES,$PROC_CPU"
}

while :
do
    print_line
    sleep 2
done
