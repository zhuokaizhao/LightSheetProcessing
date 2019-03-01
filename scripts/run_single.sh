#!/bin/bash
for ((number = 100; number<=598; number++))
do
	lsp resamp -i corr_nhdr/$number.nhdr -g grid.txt -k ctml -o resamp -v 1
done



















