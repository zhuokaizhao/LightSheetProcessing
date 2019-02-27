#!/bin/bash
for ((number = 260; number<=598; number++))
do
	lsp resamp -i corr_nhdr/$number.nhdr -g grid.txt -k box -o resamp -v 0
done



















