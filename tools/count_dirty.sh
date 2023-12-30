#!/bin/bash

log_path=log.txt
# Every 10ms
wait_sec=".01"

while true; do
	dirty="$(grep 'nr_dirty ' /proc/vmstat | awk '{print $2}')"
	echo $dirty >> $log_path
	sleep $wait_sec
done
