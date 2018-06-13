//
// Created by Jake Stover on 5/2/18.
//

#ifndef LSP_NHDRCHECK_H
#define LSP_NHDRCHECK_H

#include "CLI11.hpp"

struct nhdrCheckOptions {
    std::string path;
};

void setup_nhdr_check(CLI::App &app);

void nhdr_check_main(nhdrCheckOptions const &opt);

#endif //LSP_NHDRCHECK_H
