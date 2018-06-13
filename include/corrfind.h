//
// Created by Jake Stover on 5/9/18.
//

#ifndef LSP_CORRFIND_H
#define LSP_CORRFIND_H

#include "CLI11.hpp"


struct corrfindOptions {
    int file_number;
    std::string output_name = "-corr1.txt";
    std::vector<std::string> kernels = {"c4hexic", "c4hexicd"};
    unsigned int bound = 10;
    double epsilon = 0.00000000000001;
};

void setup_corrfind(CLI::App &app);

void corrfind_main(corrfindOptions const &opt);

#endif //LSP_CORRFIND_H
