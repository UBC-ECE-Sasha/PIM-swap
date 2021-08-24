#!/bin/bash
#
# Runs specified SSHFILE on QEMU guest and then kills buildroot

ROOT=buildroot
TERMINAL="gnome-terminal"

# command line arguments to script with defaults
HOST_SH="${1:-wiredtiger-copy.sh}" # file to run on host
GUEST_SH="${2:-wiredtiger-run-ycsb-c5.sh}" # file to run on guest
QEMU_MEM="${3:-24576}" # memory in MB of QEMU guest
CORES="${4:-4}" # number of cores to emulate
EXTRA_DRIVE="${5:-/media/jackson/WDC_HDD/disk_imgs/disk_120G.raw}" # other hard drive to add. TODO: once dev is complete, setup default so that it's not necessary
TIMEOUT="${6:-0}"

TEST_PROGRAM=(${GUEST_SH//_/ })
LOG_PREFIX="${TEST_PROGRAM[0]}_"
LOG_DIR="log"

STDOUT_LOG_SUFFIX=".log"
STDOUT_LOG_NAME="$LOG_DIR/$LOG_PREFIX$(date '+%Y-%m-%d--%H-%M-%S')$STDOUT_LOG_SUFFIX"
STDERR_LOG_SUFFIX=".err"
STDERR_LOG_NAME="$LOG_DIR/$LOG_PREFIX$(date '+%Y-%m-%d--%H-%M-%S')$STDERR_LOG_SUFFIX"

mkdir -p $LOG_DIR

echo "QEMU guest memory (mb): $QEMU_MEM" >> $STDOUT_LOG_NAME
echo "Test executiong file: $GUEST_SH" >> $STDOUT_LOG_NAME

#$TERMINAL --title="QEMU monitor" -- 
qemu-system-x86_64 -enable-kvm \
 -kernel ../output/images/bzImage \
 -initrd ../output/images/rootfs.cpio.gz \
 -append "console=ttyS0" \
 -cpu host \
 -smp $CORES \
 -m $QEMU_MEM \
 -usb \
 -chardev pty,id=ser0 \
 -serial chardev:ser0 \
 -net user,hostfwd=tcp::10022-:22 -net nic \
 -nographic \
 -drive file=../swap-10g.raw,format=raw,if=ide \
 -drive file=$EXTRA_DRIVE,format=raw,if=ide &

# wait for QEMU to start, run script on host, ssh and run guest script.
# TODO, wait on QEMU monitor (if possible) instead of sleeping
sleep 10 && \
bash $HOST_SH && \
(cat $GUEST_SH | sshpass -p "root" ssh root@localhost \
  -p 10022 \
  -o "UserKnownHostsFile /dev/null" \
  -o StrictHostKeyChecking=no) \
>> $STDOUT_LOG_NAME 2> $STDERR_LOG_NAME &

if [ $TIMEOUT -gt 0 ]
then
  sleep $TIMEOUT
  kill $(pidof qemu-system-x86_64)
fi & #TODO parallel properly