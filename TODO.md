1. Skim: Imcompete dataset(nhdr and nrrd size dismatch) -> it will cause nrrdLoad error. (Work around: fullfill empty lines with last line)            
2. corrnhdr: Now completed with ofstream, can it be done via teem/nrrd?      
3. Proj: should we seperate proj part from skim?(but it seems MUCH MORE slower). Can We fix it?   
4. Skim: projection functions prints wrong projs. (Work around: using `proj` instead) So we have to either fix it or improve `proj`     