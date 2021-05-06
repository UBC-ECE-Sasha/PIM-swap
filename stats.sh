#!/bin/bash
#
# 2020-03-29 J.Nider
#
# Preprocess the data by extracting useful information from the log file
# and writing it in a way that can be easily imported to more powerful
# tools (such as python).
#
# The log file is written by the pimswap kernel module with some important
# information in the header like this:
#
# MRAM layout:
# 0x0000 - 0x100000: reserved
# 0x100000 - 0x101000: transfer page
# 0x101004 - 0x400000: directory
# 0x400000 - 0x4000000: storage
# Allocation table size: 7696
# Total storage blocks: 61440
# Storage block size: 1024
# Using a hash table with 391741 entries
#
# The remainder of the log file consists of lines in the form:
# STATS allocs: 1107 frees: 1058 blocks: 124  bytes: 101736 
#
# where
# allocs = number of allocations (calls to frontswap_store)
# frees = number of frees (calls to frontswap_invalidate)
# loads = number of loads (calls to frontswap_load)
# blocks = number of allocated blocks after the function completes
# bytes = number of allocated bytes after the function completes

infile=$1

# generate 'utilization efficiency' by comparing total available storage to
# how much is allocated and how much of the allocated space is actually used
block_size=$(awk '/Storage block size/ { print $4}' $infile)
num_blocks=$(awk '/Total storage blocks/ { print $4}' $infile)

output="utilization-$block_size.csv"

echo "allocated_blocks,used_bytes" > $output
# use 'awk' to output the CSV file directly
awk '/STATS/ { if ($7 > 0+max) max = $7; print $7","$9 } END { print "max blocks: " max}' $infile >> $output

./graph-allocation.py -f $output -t $num_blocks -b $block_size
./graph-utilization.py -f $output -b $block_size | tee util-$block_size.out
