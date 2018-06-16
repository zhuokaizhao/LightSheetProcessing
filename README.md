# LSP
LightSheetProcessing

## Compilation
### PC(Machines installing libs directly)
    mkdir LSP-BUILD
    cd LSP-BUILD
    cmake ../LSP(Or wherever the LSP_DIR is.)
    make

### Midway(Servers using ENVIRONMENT MODULES)
    module load gcc/6.1 cmake teem libxml2 boost
    export CXX=$(which g++)
    mkdir LSP-BUILD
    cd LSP-BUILD
    cmake -d CMAKE_CXX_COMPILER:CXX ../LSP(Or wherever the LSP_DIR is.)
    make

