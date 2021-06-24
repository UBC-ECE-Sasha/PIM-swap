# run memcached on qemu and memaslap locally
CORES="${1:-4}" # number of cores to emulate
QEMU_MEM="${2:-2400}" # memory in MB of QEMU guest
MEMCACHED_MEM="${3:-1600}" # memory in MB limit for memcached
SCRIPT="memcached -u nobody -m ${MEMCACHED_MEM}"

qemu-system-x86_64 -enable-kvm \
 -kernel output/images/bzImage \
 -initrd output/images/rootfs.cpio.gz \
 -append "console=ttyS0" \
 -cpu host \
 -smp $CORES \
 -m $QEMU_MEM \
 -usb \
 -chardev pty,id=ser0 \
 -serial chardev:ser0 \
 -net user,hostfwd=tcp::10022-:22,hostfwd=tcp::11212-:11211 -net nic \
 -nographic \
 -drive file=swap-1g.raw,format=raw,if=ide &

 sleep 10 && sshpass -p "root" \
  ssh root@localhost \
  -p 10022 \
  -o "UserKnownHostsFile /dev/null"  +\
  -o StrictHostKeyChecking=no \
  "memcached --daemon -u nobody -m $MEMCACHED_MEM"
