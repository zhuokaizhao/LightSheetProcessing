# LSP
LightSheetProcessing

## Compilation
### PC(Machines installing libs directly)
    mkdir LSP-BUILD
    cd LSP-BUILD
    cmake ../LSP(Or wherever the LSP_DIR is.)
    make

### Midway(Servers using ENVIRONMENT MODULES)
1. Run "compile.sh" under `scripts/`		
2. (Optional)Write these lines at the end of your `~/.bash_profile`:	
```
    module load gcc/6.1 cmake teem libxml2 boost

	PATH=$PATH:$HOME/bin:$HOME/work/LSP-INSTALL/bin(Or wherever your bin file is.)
	export PATH
```
	Or, you may have to load modules everytime you login and call executable file via absolute path.		
