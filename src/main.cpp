//
// Created by Jake Stover on 4/10/18.
// Modified by Zhuokai Zhao
//
#include <iostream>
#include <string>

#include "CLI11.hpp"

#include "skimczi.h"
#include "proj.h"
#include "anim.h"
#include "nhdrCheck.h"
#include "untext.h"
#include "corr.h"
#include "corrimg.h"
#include "corrfind.h"
#include "corrnhdr.h"
//#include "pack.h"
#include "start.h"


int main(int argc, char** argv) {
    CLI::App app{"Collection of utilities for processing of lightsheet data"};

    // standard process which includes skim, proj and anim
    start_standard_process(app);

    // getting information out of CZI files, generate .nhdr NRRD header files
    setup_skim(app);
    // Create projection files based on nhdr files
    setup_proj(app);
    // Create animations from projection files
    setup_anim(app);
    
    // Creates line graph summary of nhdr files
    setup_nhdr_check(app);
    // Remove grid texture from a projection
    setup_untext(app);
    
    // Measure 2D image correlation over a 2D array of offsets; 
    // Construct image to be used for cross correlation
    setup_corrimg(app);
    // Computes the shift between file specified by <file_number> and <file_number>-1, calls setup_corr
    setup_corrfind(app);
    // Prints out offset coordinates that maximized the cross correlation
    setup_corr(app);
    
    // Apply the corrections calculated by corrimg and corrfind
    setup_corrnhdr(app);
    // Process dataset with standard format
    //setup_pack(app);

    CLI11_PARSE(app, argc, argv);

    if (argc == 1) 
    {
        std::cout << app.help();
    }

    return 0;
}
