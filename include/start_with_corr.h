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
    // images that are generated from each projection file
    string image_path;
    // correlation results between each image
    string align_path;
    // new NHDR headers generated from old NHDR headers and correlation results
    string new_nhdr_path;
    // new projection files generated from new NHDR headers
    string new_proj_path;
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

    // from corrimgOptions
    string input_file;
    string output_file;
    string kernel_corrimg = "Gauss:10,4";

    // from corrfindOptions
    vector<string> input_images;
    vector< vector< pair<int, string> > > inputImages;
    vector<string> kernel_corrfind = {"c4hexic", "c4hexicd"};
    unsigned int bound = 10;
    double epsilon = 0.00000000000001;

    // from corrnhdrOptions
    int num;

    // from animOptions
    int fps = 10;
    vector< pair<int, string> > allValidFiles;
    uint tmax;
    uint dwn_sample = 2;
    double scale_x = 1.0;
    double scale_z = 1.0;
};

void start_standard_process_with_corr(CLI::App &app);

class StartwithCorr {
public:
    StartwithCorr(startwithcorrOptions const &opt = startwithcorrOptions());
    ~StartwithCorr();

    void main();

};

#endif //LSP_START_WITH_CORR_H