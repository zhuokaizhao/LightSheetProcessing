#! /bin/bash

rm -f memory.log

while true;
do
	free >> memory.log
	sleep 10;
done