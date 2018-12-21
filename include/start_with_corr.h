// The program runs skim, proj and anim in LSP
// Created by Zhuokai Zhao
// Contact: zhuokai@uchicago.edu

#ifndef LSP_START_WITH_CORR_H
#define LSP_START_WITH_CORR_H

#include <tiff.h>
#include <cstdint>
#include <string>
#include <vector>

#include "CLI11.hpp"

#include "skimczi.h"
#include "proj.h"
#include "anim.h"

using namespace std;

// Note that this only provides a standard process, limited options
// all the options for Start
struct startwithcorrOptions {
    // path that contains input czi files
    string czi_path;
    // nhdr output(for skim) and input(for proj) path
    string nhdr_path;
    // proj output(for proj) and input(for anim) path
    string proj_path;
    // anim output path
    string anim_path;

    // optional input for anim, to process a specific number of files
    string maxFileNum;
    int verbose = 0;

    // from skimOptions
    string nhdr_out_name;
    string xml_out_name;
    string file;

    // from projOptions
    string file_name;
    int file_number;
    int number_of_processed = 0;

    // from animOptions
    int fps = 10;
    vector< pair<int, string> > allValidFiles;
    uint tmax;
    uint dwn_sample = 2;
    double scale_x = 1.0;
    double scale_z = 1.0;
};

void start_standard_process(CLI::App &app);

class Start {
public:
    Start(startOptions const &opt = startOptions());
    ~Start();

    void main();

};

#endif //LSP_START_H