#!/bin/bash
for ((number = 1; number<=9; number++))
do
	lsp resamp -i corr_nhdr/00$number.nhdr -g grid.txt -k ctml -o resamp_ctml -v 1
done



















