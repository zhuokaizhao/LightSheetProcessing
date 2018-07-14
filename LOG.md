06/14/2018		
Started workint on lsp project.		
1. Downloaded Jacob's source code from git.		
2. Re-writed "cmake" file to make use of midway's module system. See README for building instructions.

06/19/2018
1. Added some documentations for Jacob's code.        
2. Debuged under gcc/6.1     

06/21/2018
Fixed skim. Now it can generate headers and projection files correctly.

06/23/2018
Refined cmake. Now it has great file structures.     

06/25/2018
Rewrited code to a more OOP style.		

06/26/2018
Finished rewrite corr*.cpp files

06/27/2018      
1. Fixed small bugs in crr*.cpp files     
2. Completed `standard` subcommand, which can set correct arguments and run processing automatically.    

06/28/2018
1. Renamed `standard` to `pack`.     
2. Rewrited `skim`.      
3. Wrote a tmp work around for `skim` in the case that data is broken(nrrd header and data not match)       

06/30/2018
Coded `anim`. Everthing is done except for resampling. Without resampling, the program will use too much memory.     

07/01/2018
Coded `proj` for build projections based on nhdr file.

07/02/2018
Did correctness check.  
1. `proj` prints correct projs but skim does not. 
2. `anim` works well but I have to face the resampling promblem.    

07/04/2018  
1. Completed `anim` lib(resampling part).        
2. `proj` in fact returns bad results because bad points(abnormal peaks) in original dataset.       

07/06/2018
1. Rolled `proj` back. Combined projection functions back into `skim`.  
2. Fixed bugs in `skim`, it can generate correct projs now.		

07/09/2018
1. Fixed bugs in `corrnhdr`.		
2. Added functionality in `anim`. It can crop nrrd arrays based on space origin now. So the anim after `corr` can be generated correctly.		

07/11/2018
Learnt FFT and libfftw3.		

07/12/2018
Built `untext`. In `untext`, the masking algo is brutal and does not work well for xz projs.

07/13/2018
Fixed bugs in `untext` and paralleled it. It works for all projs now, but masking algo still need to be optimized.