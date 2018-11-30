#!/bin/bash
#SBATCH --job-name=generate_proj
#SBATCH --output=generate_proj.out
#SBATCH --error=generate_proj.err
#SBATCH --partition=broadwl
#SBATCH --time=05:00:00
#SBATCH --nodes=1
#SBATCH --ntasks-per-node=28
#SBATCH --mem-per-cpu=1024

#Load any necessary modules here
module load boost/1.62.0 gcc/6.1 libpng/1.5 cmake teem libxml2 opencv/3.1 openmpi fftw3

./lsp proj -i nhdr/ -o proj_sbatch/ -b 181113 -v 0