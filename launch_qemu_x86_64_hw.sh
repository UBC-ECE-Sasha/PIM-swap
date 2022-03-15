#!/bin/bash

# Setting the ethenet connection
# On the host:
# sudo apt install uml-utilities (for tunectl)
# On the target
# $ ifconfig eth0 192.168.7.2

NR_RANKS=1
DOWNLOAD=0
IMAGES_DIR="."

# Redirects tput to stderr, and drop any error messages.
tput2() {
	tput "$@" 1>&2 2>/dev/null || true
}

error() {
	tput2 bold && tput2 setaf 1
	echo "ERROR: $*" >&2
	tput2 sgr0
}

warn() {
	tput2 bold && tput2 setaf 3
	echo "WARNING: $*" >&2
	tput2 sgr0
}

info() {
    tput2 bold
    echo "$*" >&2
    tput2 sgr0
}

die() {
	[ -z "$*" ] || error "$@"
	exit 1
}

usage() {
    echo "Usage: sudo $(basename $0) [-h] [-n NR_RANKS] [-w] [-d IMAGES_DIR]"
    echo "  -h|--help: Print this help and exit"
    echo "  -n|--nr-ranks: Number of ranks to map"
    echo "    Default to 1"
    echo "  -w|--download: Download the kernel and rootfs images"
    echo "  -i|--images: Kernel and rootfs images directory"
    echo "    Default to current directory"
}


#
# Parse and validate argument
#
while :; do
    case "$1" in
        -h|--help)
            usage
            exit
            ;;
        -n|--nr-ranks)
            NR_RANKS="$2"
            shift
            ;;
        -w|--download)
            DOWNLOAD=1
            ;;
        -d|--images)
            IMAGES_DIR="$2"
            shift
            ;;
        -?*)
            warn "Unkown option (ignored): $1"
            ;;
        *) # no more option
            break
            ;;
    esac
    shift
done

if [ $EUID -ne 0 ]; then
    die "This script must be run with root privileges"
fi

#
# Download images if needed
#
if [ $DOWNLOAD -eq 1 ]; then
    if [ ! -d "$IMAGES_DIR" ]; then
        mkdir -p $IMAGES_DIR
    fi
    scp upmem@upmemsrv7:/var/lib/jenkins/workspace/BuildBuildrootQEMUImage/buildroot/build_qemu_x86_64/images/bzImage $IMAGES_DIR/
    scp upmem@upmemsrv7:/var/lib/jenkins/workspace/BuildBuildrootQEMUImage/buildroot/build_qemu_x86_64/images/rootfs.ext2 $IMAGES_DIR/

fi

#
# Compute needed memory and mapping options
#
RAM_SIZE=100
MAPPING_OPTIONS=""

id=0                                                                             
while [ $id -lt $NR_RANKS ]; do 
    if [ ! -c "/dev/dax$id.$id" ]; then
        die "Unable to map $NR_RANKS ranks"
    fi
    # Add 200M per rank
    RAM_SIZE=$((RAM_SIZE + 200))
    MAPPING_OPTIONS="$MAPPING_OPTIONS --object memory-backend-file,id=pim$id,size=8G,mem-path=/dev/dax$id.$id,align=1G --device pc-dimm,id=dimm$id,memdev=pim$id"
    id=$((id + 1))
done


#
# Setup tap interface to ssh into the VM
#
sudo ./runqemu-ifup 1000 1000

sudo ifconfig tap0 192.168.7.1
#
# Start VM
#
NR_SLOTS=$((NR_RANKS + 1))
MAX_SIZE=$((MRAM_SIZE + 8912 * NR_RANKS))

qemu-system-x86_64 -M pc \
 -m ${RAM_SIZE}M,slots=$NR_SLOTS,maxmem=${MAX_SIZE}M $MAPPING_OPTIONS \
 -kernel $IMAGES_DIR/bzImage -nographic -serial mon:stdio -append "rootwait root=/dev/vda console=ttyS0 nokaslr" \
 -drive if=virtio,file=$IMAGES_DIR/rootfs.ext2,format=raw \
 -device e1000,netdev=net0,mac=52:55:00:d1:55:01 -netdev tap,id=net0,ifname=tap0,script=no,downscript=no \
 -enable-kvm -s

#
# Cleanup tap interface
#
sudo ./runqemu-ifdown tap0
