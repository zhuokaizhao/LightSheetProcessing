#!/bin/bash
for ((number = 32; number<=99; number++))
do
	lsp resamp -i resamp_box/0$number.nhdr -m VideoOnly -g grid.txt -k box -o resamp3D -v 1
done



















