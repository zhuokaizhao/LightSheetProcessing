// The program runs skim, proj and anim in LSP
// Created by Zhuokai Zhao
// Contact: zhuokai@uchicago.edu

#include <teem/nrrd.h>
#include "start.h"
#include <chrono> 
#include "util.h"

using namespace std;

void start_standard_process(CLI::App &app) {
    auto opt = make_shared<startOptions>();
    auto sub = app.add_subcommand("start", "Stand process that includes skim, proj and anim. Limited options though.");

    // czi path
    sub->add_option("-c, --czi_path", opt->czi_path, "Path for all the czi files")->required();
    // nhdr path
    sub->add_option("-n, --nhdr_path", opt->nhdr_path, "Path for nhdr header files")->required();
    // proj path
    sub->add_option("-p, --proj_path", opt->proj_path, "Path for projection files")->required();
    // anim path
    sub->add_option("-a, --anim_path", opt->anim_path, "Path for anim results which includes images and videos")->required();
    // optional input if we just want to process a specific number of files
    sub->add_option("-f, --num_files", opt->maxFileNum, "Max number for files that we want to process");
    // verbose
    sub->add_option("-v, --verbose", opt->verbose, "Progress printed in terminal or not");

    sub->set_callback([opt]()
    {
        try
        {
            auto start = chrono::high_resolution_clock::now();

            // run LSP SKIM
            // construct options for skim
            auto opt_skim = std::make_shared<skimOptions>();
            // skim requires input czi file input path and nhdr output path
            opt_skim->input_path = opt->czi_path;
            opt_skim->nhdr_out_name = opt->nhdr_path;
            opt_skim->verbose = opt->verbose;
            Skim(*opt_skim).main();

            // run LSP PROJ
            // construct options for LSP
            auto opt_proj = std::make_shared<projOptions>();
            // proj requires input czi, nhdr file path and output proj path
            opt_proj->nhdr_path = opt->nhdr_path;
            opt_proj->proj_path = opt->proj_path;
            opt_proj->verbose = opt->verbose;
            Proj(*opt_proj).main();

            // run LSP ANIM
            //Anim::Anim(*opt).main();
            auto stop = chrono::high_resolution_clock::now(); 
            auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 
            cout << endl << "Processing took " << duration.count() << " seconds" << endl << endl; 
        }
        catch(LSPException &e) 
        {
            std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
        }
    });
}


