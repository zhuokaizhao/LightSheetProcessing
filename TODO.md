1. Skim: Imcompete dataset(nhdr and nrrd size dismatch) -> it will cause nrrdLoad error. (Work around: fullfill empty lines with last line)       
2. Anim: Resampling fails.      
3. corrnhdr: copy header infos correctly, but data paths are wrong.     
4. Proj: should we seperate proj part from skim?(but it seems MUCH MORE slower).    