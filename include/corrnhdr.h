//! \file corrnhdr.h
//! \author Jake Stover
//! \date 2018-05-09
//! \brief Load and apply the related offsets for input file.

#ifndef LSP_CORRNHDR_H
#define LSP_CORRNHDR_H


#include "CLI11.hpp"

struct corrnhdrOptions {
    int num;
};

void setup_corrnhdr(CLI::App &app);

void corrnhdr_main(corrnhdrOptions const &opt);


#endif //LSP_CORRNHDR_H
