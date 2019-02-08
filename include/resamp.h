// The program performs resampling process on images genereated by anim.cpp
// Created by Zhuokai Zhao
// Contact: zhuokai@uchicago.edu


#ifndef LSP_RESAMP_H
#define LSP_RESAMP_H

#include <vector>
#include "CLI11.hpp"
#include "lsp_math.h"
#include "image.h"

using namespace std;

struct resampOptions {
    // path that includes all the input images obtained from anim
    string image_path;
    // image types (max or avg)
    string image_type;
    // restrict the number of files that we processed
    string maxFileNum;
    // path that includes all the output images
    string out_path;
    // path of the kernel file
    string kernel_path;

    // vector of pair that includes the file sequence numbers and image names
    vector< pair<int, string> > allValidImages;

    uint imageNum;
    
    uint verbose = 0;
};

void setup_resamp(CLI::App &app);

class Resamp {
    public:
        Resamp(resampOptions const &opt = resampOptions());
        ~Resamp();

        void main();
    
    private:
        // evaluate convolution between image and kernel
        void ConvoEval(lspCtx *ctx, double xw, double yw);
        airArray* mop;
};

#endif //LSP_RESAMP_H