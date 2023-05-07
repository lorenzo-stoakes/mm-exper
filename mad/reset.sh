#!/bin/bash

function get_chars()
{
	for f in $(seq 16384); do
		echo -n $((f % 10))
	done
}

get_chars > test.txt
