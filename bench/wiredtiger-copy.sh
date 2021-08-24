#!/bin/bash
#
# 2021-08-11 J. Dagger
#
# Copy built wiredtiger executables over to QEMU guest

cd ../../

# TODO build wt if not built

sshpass -p "root" ssh root@localhost -p 10022 -o "UserKnownHostsFile /dev/null" -o StrictHostKeyChecking=no 'mkdir wiredtiger && mkdir wiredtiger/build_posix'
sshpass -p "root" scp -o "UserKnownHostsFile /dev/null" -o StrictHostKeyChecking=no -P 10022 -r wiredtiger/bench/ root@localhost:wiredtiger/
sshpass -p "root" scp -o "UserKnownHostsFile /dev/null" -o StrictHostKeyChecking=no -P 10022 -r wiredtiger/build_posix/.libs/ root@localhost:wiredtiger/build_posix/
sshpass -p "root" scp -o "UserKnownHostsFile /dev/null" -o StrictHostKeyChecking=no -P 10022 -r wiredtiger/build_posix/bench/ root@localhost:wiredtiger/build_posix/

# sshpass -p "root" scp -o "UserKnownHostsFile /dev/null" -o StrictHostKeyChecking=no -P 10022 -r root@localhost:/dev/wiredtiger/build_posix/benhc/wtperf/WT_TEST/monitor /monitor