#!/bin/bash

log_path=log.txt
# Every 10ms
wait_sec=".01"

while true; do
	dirty="$(grep Dirty /proc/meminfo | awk '{print $2}')"
	echo $dirty >> $log_path
	sleep $wait_sec
done
