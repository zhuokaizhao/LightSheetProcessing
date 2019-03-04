#!/bin/bash
for ((number = 10; number<=99; number++))
do
	lsp resamp -i corr_nhdr/0$number.nhdr -g grid.txt -k ctml -o resamp_ctml -v 1
done



















