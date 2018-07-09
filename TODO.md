Problem to discuss
1. Skim: Imcompete dataset(nhdr and nrrd size dismatch) -> it will cause nrrdLoad error. (Work around: fullfill empty lines with last line)            
2. corrnhdr: Now completed with ofstream, can it be done via teem/nrrd?      
3. Proj: It is MUCH MORE slower than generating projs along with `skim`. Can We fix it? Or do we really need a proj lib?    
4. Nrrd: When loading a large file, it seems cannot get data into array 100% correctly. (Go and clear up the mess in your Desktop!) 

TODO:
1. corrnhdr: bugs from offset_bound to end