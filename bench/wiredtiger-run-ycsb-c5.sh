#!/bin/sh
#
# 2021-08-19 J. Dagger
#
# Execute wtperf on QEMU and then shutdown machine

chmod +x mount_sdb.sh
./mount_sdb.sh

mv wiredtiger /media/wiredtiger # move to larger shared drive
cd /media/wiredtiger/build_posix/bench/wtperf
./wtperf -O ../../../bench/wtperf/runners/ycsb-c5.wtperf

echo MONITOR
cat WT_TEST/monitor
echo CSV DONE
