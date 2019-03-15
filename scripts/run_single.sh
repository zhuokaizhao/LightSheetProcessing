#!/bin/bash
for ((number = 1; number<=9; number++))
do
	lsp resamp -i resamp_box/00$number.nhdr -m VideoOnly -g grid.txt -k box -o resamp3D_v5 -v 0
done
for ((number = 10; number<=99; number++))
do
	lsp resamp -i resamp_box/0$number.nhdr -m VideoOnly -g grid.txt -k box -o resamp3D_v5 -v 0
done
for ((number = 100; number<=598; number++))
do
	lsp resamp -i resamp_box/$number.nhdr -m VideoOnly -g grid.txt -k box -o resamp3D_v5 -v 0
done
lsp resamp -i resamp_box/ -m VideoOnly -g grid.txt -k box -o resamp3D_v5 -v 0



















