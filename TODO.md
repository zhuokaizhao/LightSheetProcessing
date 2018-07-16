TODO:
1. untext:  improve masking algo.
2. untext:  bad ft results if szx!=szy

Slide works:
1. Skim: Imcompete dataset(nhdr and nrrd size dismatch) -> it will cause nrrdLoad error. (Work around: fullfill empty lines with last line)            
2. corrnhdr: Now completed with ofstream, can it be done via teem/nrrd?      
3. Proj: It is MUCH MORE slower than generating projs along with `skim`. Can We fix it? Or do we really need a proj lib?    
4. Nrrd: When loading a large file, it seems cannot get data into array 100% correctly.
