#!/bin/bash
set -e; set -o pipefail

inode=$(ls -i readahead.dat | awk '{print $1}')

function drop_caches()
{
	echo 3 | sudo tee /proc/sys/vm/drop_caches >/dev/null
}


function do_looped()
{
	for count in $(seq 256); do
		drop_caches

		i=1

		while [[ $i -le 256 ]]; do
			echo "--- $((i - 1)) / $count ---"
			sudo dmesg -C
			sudo ./readahead $((i - 1)) $count
			dmesg | grep IVG | grep $inode || true
			echo "-------------------"
			i=$((i+$count))
		done
	done
}

function do_unlooped()
{
	for count in $(seq 256); do
		drop_caches

		echo "--- count=$count ---"
		sudo dmesg -C
		sudo ./readahead $count
		dmesg | grep IVG | grep $inode || true
		echo "-------------------"
	done
}

do_unlooped
