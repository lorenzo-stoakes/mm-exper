#!/bin/bash
set -e; set -o pipefail

inode=$(ls -i readahead.dat | awk '{print $1}')

function drop_caches()
{
	echo 3 | sudo tee /proc/sys/vm/drop_caches >/dev/null
}

for count in $(seq 256); do
	drop_caches

	i=$count

	while [[ $i -lt 256 ]]; do
		echo "--- $i / $count ---"
		sudo dmesg -C
		sudo ./readahead $count
		dmesg | grep IVG | grep $inode
		echo "-------------------"
		i=$((i+$count))
	done
done
