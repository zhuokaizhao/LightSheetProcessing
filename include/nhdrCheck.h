//! \file nhdrCheck.h
//! \author Jake Stover
//! \date 2018-05-02
//! \brief Check if the input file exists.

#ifndef LSP_NHDRCHECK_H
#define LSP_NHDRCHECK_H

#include "CLI11.hpp"

struct nhdrCheckOptions {
    std::string path;
};

void setup_nhdr_check(CLI::App &app);

void nhdr_check_main(nhdrCheckOptions const &opt);

#endif //LSP_NHDRCHECK_H
