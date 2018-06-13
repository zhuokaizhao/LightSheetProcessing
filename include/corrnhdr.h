//
// Created by Jake Stover on 5/9/18.
//

#ifndef LSP_CORRNHDR_H
#define LSP_CORRNHDR_H


#include "CLI11.hpp"

struct corrnhdrOptions {
    int num;
};

void setup_corrnhdr(CLI::App &app);

void corrnhdr_main(corrnhdrOptions const &opt);


#endif //LSP_CORRNHDR_H
