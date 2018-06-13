//
// Created by Jake Stover on 5/9/18.
//

#ifndef LSP_UNTEXT_H
#define LSP_UNTEXT_H

#include "CLI11.hpp"

struct untextOptions {
    std::string filename;
};

void setup_untext(CLI::App &app);

void untext_main(untextOptions const &opt);

#endif //LSP_UNTEXT_H
