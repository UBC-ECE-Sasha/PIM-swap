#!/bin/sh
#
# 2021-09-29 J. Dagger
#
# Output memory usage statistics approximately twice a second

echo "date,free mem(kB),free swap(kB),cached swap(kB)"

print_line(){
    FREE_MEM=$(awk '/MemFree/{print $(NF-1)}' /proc/meminfo)
    FREE_SWAP=$(awk '/SwapFree/{print $(NF-1)}' /proc/meminfo)
    CACHED_SWAP=$(awk '/SwapCached/{print $(NF-1)}' /proc/meminfo)
    echo "$(date +"%Y/%m/%d-%T"),$FREE_MEM,$FREE_SWAP,$CACHED_SWAP"
}

while :
do
    print_line
    sleep 0.5
done
