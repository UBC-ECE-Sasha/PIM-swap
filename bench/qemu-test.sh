#!/bin/bash
#
# 2021-08-9 J. Dagger, J. Ramsden
#
# Runs the QEMU guest with the ability to pass scripts to be run
# locally and on the guest as parameters.
# 
# Port 10022 must be free for ssh meaning multiple instances of this script cannot be run concurrently

ROOT=buildroot
TERMINAL="gnome-terminal"

# command line options defaults
HOST_SH="none" # file to run on host
GUEST_SH="none" # file to run on guest
QEMU_MEM="4096" # memory in MB of QEMU guest
CORES="1" # number of cores to emulate
EXTRA_DRIVE="none" # other hard drive to add. TODO: once dev is complete, setup default so that it's not necessary
TIMEOUT="0" # in seconds
KILL_AFTER="false" # whether or not to kill machine after running GUEST_SH. Does nothing if GUEST_SH is 'none'

print_usage() {
  echo "usage: $0 [-i pathtohostfile] [-g pathtoguestfile] [-m memorymb] [-c simulatedcores] [-d pathtodiskimg] [-t timeout(s)] [-e] [-k]"
  echo "-i pathtohostfile: relative path to initial file to run on host (default: ${HOST_SH})"
  echo "-g pathtoguestfile: relative path of file to run test and collect data on guest (default: ${GUEST_SH})"
  echo "-m memorymb: memory in MB for QEMU guest (default: ${QEMU_MEM})"
  echo "-c simulatedcores: simulated cores for QEMU guest (default: ${CORES})"
  echo "-d pathtodiskimg: path to extra disk image (leave out if just using rootfs) (default: ${EXTRA_DRIVE})"
  echo "-t timeout(s): QEMU guest (not test) timeout in seconds with 0 for no timeout (default: ${TIMEOUT})"
  echo "-e: kill after running test (default: QEMU stays running)"
  echo "-k: kill all running QEMU processes on machine and do nothing else"
  echo "-h: print this message"
  echo "read more at https://wiki.ubc.ca/PIM-SWAP"
}

kill_qemu() {
  echo "Killing QEMU"
  kill -9  $(pidof qemu-system-x86_64) # kills hung up qemu instances
  echo "All QEMU instances killed"
  exit 0
} 

while getopts 'i:g:m:c:d:t:ekh' flag; do
  case "${flag}" in
    i) HOST_SH="${OPTARG}" ;;
    g) GUEST_SH="${OPTARG}" ;;
    m) QEMU_MEM="${OPTARG}" ;;
    c) CORES="${OPTARG}" ;;
    d) EXTRA_DRIVE="${OPTARG}" ;;
    t) TIMEOUT="${OPTARG}" ;;
    e) KILL_AFTER='true' ;;
    k) kill_qemu ;;
    h) print_usage
       exit 1 ;;
    *) print_usage
       exit 1 ;;
  esac
done

# logs are named based on the program being tested, memory allocated and datetime
PROG_NAME=$(basename $GUEST_SH)
TEST_PROGRAM=${PROG_NAME//_/ }
LOG_PREFIX="${TEST_PROGRAM[0]}_${QEMU_MEM}MB_"
LOG_DIR="logs"

STDOUT_LOG_SUFFIX=".log"
STDOUT_LOG_NAME="$LOG_DIR/$LOG_PREFIX$(date '+%Y-%m-%d--%H-%M-%S')$STDOUT_LOG_SUFFIX"
STDERR_LOG_SUFFIX=".err"
STDERR_LOG_NAME="$LOG_DIR/$LOG_PREFIX$(date '+%Y-%m-%d--%H-%M-%S')$STDERR_LOG_SUFFIX"

mkdir -p $LOG_DIR

echo "QEMU guest memory (mb): $QEMU_MEM" >> $STDOUT_LOG_NAME
echo "Test execution file: $GUEST_SH" >> $STDOUT_LOG_NAME

# wait for QEMU to start, run script on host, ssh and run guest script.
# TODO, wait on QEMU monitor (if possible) instead of sleeping
(sleep 10
(if [ $HOST_SH != "none" ] # run HOST_SH if it exists
  then 
  bash $HOST_SH
fi && \
if [ $GUEST_SH != "none" ] # run GUEST_SH on guest if it exists
then
  cat $GUEST_SH
  if [ $KILL_AFTER == "true" ]
    then
    echo poweroff -f # poweroff after running GUEST_SH if KILL_AFTER is enabled
  fi
fi | sshpass -p "root" ssh root@localhost \
  -p 10022 \
  -o "UserKnownHostsFile /dev/null" \
  -o StrictHostKeyChecking=no) \
>> $STDOUT_LOG_NAME 2> $STDERR_LOG_NAME ) &

timeout $TIMEOUT qemu-system-x86_64 -enable-kvm \
 -kernel ../output/images/bzImage \
 -initrd ../output/images/rootfs.cpio.gz \
 -append "console=ttyS0" \
 -cpu host \
 -smp $CORES \
 -m $QEMU_MEM \
 -usb \
 -chardev pty,id=ser0 \
 -serial chardev:ser0 \
 -nographic \
 -net user,hostfwd=tcp::10022-:22 -net nic \
 -drive file=../swap-20g.raw,format=raw,if=ide \
 `if [ $EXTRA_DRIVE != "none" ]; then echo "-drive file=$EXTRA_DRIVE,format=raw,if=ide"; fi` &

wait

# there's a typo in wt monitor files
sed -i 's/(uS)read/(uS),read/g' $STDOUT_LOG_NAME
