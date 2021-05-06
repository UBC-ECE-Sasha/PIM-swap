#!/bin/bash

sizes="64 128 256 512 1024 2048"

for s in $sizes; do
	file="log-$s.txt"
	awk '{ if ($11 > 0+pages) pages++; } END { print pages }' $file
done
