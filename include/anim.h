//
// Created by Jake Stover on 4/24/18.
//

#ifndef LSP_AMIN_H
#define LSP_AMIN_H

#include <CLI11.hpp>

struct AnimOptions {
    std::string name;
    uint tmax;
    uint dwn_sample;    // How much to down-sample
    float scale_x;
    float scale_z;
};

void setup_anim(CLI::App &app);
int anim_main(AnimOptions const &opt);


#endif //LSP_AMIN_H
