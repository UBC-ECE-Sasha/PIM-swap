#!/bin/bash

sizes="64 128 256 512 1024 2048"

for s in $sizes; do
	file="log-$s.txt"
	outfile="total-eff-$s.txt"
	total=$(awk '/Total storage blocks/ { print $4 }' $file)
	echo "Total storage blocks: $total"
	awk '/FULL/ { print $3 }' $file > $outfile
done
