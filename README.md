# LSP
LightSheetProcessing

## Compilation
    Run "compile.sh" under `scripts/`		

    P.S. it would modify your '~/.bash_profile' and '~/.profile'

## Running
1. Print `lsp -h` for help.        
2. For well formatted experiment data, I highly recommand `standard` subcommand. It would automatically run data processing with correct arguments.
3. A standard dataset is:   
    a. original data is in 'some_path/czi' folder.  
    b. data files are named: some_name.czi, some_name(1).czi, some_name(2).czi...    
    c. this driver will generate work folders under "some_path/" and output files in there. 
 