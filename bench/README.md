# PIM-swap benchmarking
The following are instructions for adding applications to PIM-swap benchmarking and some examples. The tests are logged in the logs directory. For more details on the design of the benchmarking suite, go to [the wiki.](https://wiki.ubc.ca/PIM-SWAP)

## Applications
Benchmarking PIM-swap supports the following applications with their associated identifying log prefixes.
- [wiredtiger](https://www.mongodb.com/docs/manual/core/wiredtiger/) (WT)

## Adding an application
The benchmarking framework is designed to be extendable to various applications. The following instructions walk through adding an application. All relevant locations in the code are marked with the comment `# ADD APPLICATION HERE (STEP X)` where X is the step number in the corresponding set of instructions.
1. Pick an two letter prefix for the application log directories and add it to the list in the section above.
2. Create a subdirectory for the application in the conf directory. This directory should contain any relevant build scripts and a subdirectory, called "configs" which contains any created test configuration scripts. The application should be cloned into and built in a subdirectory of bench. The build scripts should specify a commit hash so that all subsequent tests will be on the same code.
3. Add the "bench/[application]" directory into the gitignore.
4. Create and document "[application]-test.sh" script which runs a test of the given application. Your script should put all generated log files in a log directory that starts with the same two-letter prefix you selected in step 1.
5. Create a function in parse-logs.py to parse whatever performance logs your application outputs into a dataframe.
6. In parse-logs.py, add code to detect log files from your application and call the function you wrote in step 5.
7. Add names of columns that should be plotted from the dtaframe generated in step 5 to the 'appPerfCols' list in plot-logs.py.
8. Create various experiment scripts that also call limit-mem.sh.
9. Email me when something I didn't foresee comes up and my scripts break.

## Examples

### Setup
The script setup-machine.sh installs [sysstat](https://github.com/sysstat/sysstat), which is used for logging and creates a directory for the ramdisk with the appropriate permissions. 

### Running locally
First, to limit memory to 16 GiB, we use limit-mem.sh.
```
sudo ./limit-mem.sh -m 16384 -d /scratch/ramdisk
```
Next, we decide to run wiredtiger with the config file "ycsb-c_32G.wtperf" which is the standard YCSB-C with a 32G cache.
```
./wtperf-test.sh conf/wiredtiger/configs/ycsb-c_32G.wtperf
```
The script should create a log directory with the name logs/WT_ycsb-c_32G.wtperf_2022-04-04--21-07-58 with the datetime replaced with whenever we run it.

### Parsing and plotting logs
We then parse the log files into a single csv with the following command.
```
python3 parse-logs.py logs/WT_ycsb-c_32G.wtperf_2022-04-04--21-07-58
```
Now, there should be an output.csv file in the given log directory. It will contain a concatenated timeseries dataframe containing data from vmstat, pidstat and application. Of note, it adjusts all data to a time-series period of 1s.  
To plot the data we parsed above, we call parse-logs.py. The options are better described in the file but the following will make a plot of memory and swap statistics and app performance with a 120s averaged period.
```
python3 plot-logs.py -m -a -s -r 120 logs/WT_ycsb-c_32G.wtperf_2022-04-04--21-07-58/output.csv
```

### Running on QEMU guest
PIM-swap can be setup to run on a Buildroot/QEMU virtual environment. To benchmark on a QEMU guest, you will need QEMU and sudo acces to QEMU. The script qemu-test.sh allows the user to specify the test configuration, amount of memory for QEMU to give the guest and the number of cores to emulate amongst other things. The following is an example of benchmarking PIM-swap with a wtperf config named ycsb-c_10m_16G.wtperf, 16G of memory, an additional drive and a swapfile.  
```
sudo ./qemu-test.sh -f ycsb-c_10m_16G.wtperf -d /mnt/wd_hdd_350g/disk_160G.raw -s /mnt/wd_hdd_350g/swap-32g.raw -e
```
