#!/bin/sh
#
# 2021-08-19 J. Dagger
#
# Execute wtperf (ycsb-ca) on a running QEMU guest started by
# qemu-test.sh
# Will fail if QEMU guest isn't running or blocks connections
# Assumes SSH port of 10022

chmod +x mount_sdb.sh
./mount_sdb.sh

mv wiredtiger /media/wiredtiger # move to larger shared drive
cd /media/wiredtiger/build/bench/wtperf/wtperf
./wtperf -O ../../../bench/wtperf/runners/$CONFIG

echo MONITOR CSV
cat WT_TEST/monitor
echo CSV DONE
