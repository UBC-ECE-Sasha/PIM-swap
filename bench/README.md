# PIM-swap benchmarking
The following are instructions for benchmarking PIM-swap with real-world workloads in wiredtiger locally and using QEMU. In both cases, the tests are logged in the logs directory.

## Running on QEMU guest
PIM-swap can be setup to run on a Buildroot/QEMU virtual environment. To benchmark on a QEMU guest, you will need QEMU and sudo acces to QEMU. The script qemu-test.sh allows the user to specify the test configuration, amount of memory for QEMU to give the guest and the number of cores to emulate amongst other things. The following is an example of benchmarking PIM-swap with a wtperf config named ycsb-c_10m_16G.wtperf, 16G of memory, an additional drive and a swapfile.  
```sudo ./qemu-test.sh -f ycsb-c_10m_16G.wtperf -d /mnt/wd_hdd_350g/disk_160G.raw -s /mnt/wd_hdd_350g/swap-32g.raw -e```

## Running locally
Currently, the script local-test.sh can be run to benchmark PIM-swap locally. This script artificially reduces RAM by creating a ramfs. To use it, you will need sudo access to mount, umount and modprobe.