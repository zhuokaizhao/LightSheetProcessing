//
// Created by Jake Stover on 5/9/18.
//

#ifndef LSP_CORRIMG_H
#define LSP_CORRIMG_H

#include "CLI11.hpp"

struct corrimgOptions {
    std::string input_file;
    std::string output_file;
    std::string kernel = "Gauss:10,4";
};

void setup_corrimg(CLI::App &app);

void corrimg_main(corrimgOptions const &opt);

#endif //LSP_CORRIMG_H
