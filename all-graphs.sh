#!/bin/bash

# assumes log files name log-<size>.txt

outfile="utilization-by-block_size.csv"

echo "blocks_allocated,percent_used" > $outfile
for i in 64 128 256 512 1024 2048; do
	./stats.sh log-$i.txt

	mean=$(awk '/^Mean/ { print $2 }' util-$i.out)
	echo "$i,$mean" >> $outfile
done

# create the final graph
./graph-utilization-by-size.py -f $outfile
