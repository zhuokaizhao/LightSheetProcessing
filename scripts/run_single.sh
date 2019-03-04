#!/bin/bash
for ((number = 565; number<=574; number++))
do
	lsp resamp -i corr_nhdr/$number.nhdr -g grid.txt -k ctml -o resamp_ctml -v 1
done



















