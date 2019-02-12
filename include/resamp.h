// The program performs resampling process on images genereated by anim.cpp
// Created by Zhuokai Zhao
// Contact: zhuokai@uchicago.edu


#ifndef LSP_RESAMP_H
#define LSP_RESAMP_H

#include <vector>
#include "CLI11.hpp"
#include "lsp_math.h"
#include "image.h"
#include "volume.h"

using namespace std;

struct resampOptions {
    // path that includes all the input images obtained from anim
    //string image_path;

    // image types (max or avg)
    // string image_type;

    // total number of images
    // uint imageNum;

    // vector of pair that includes the file sequence numbers and image names
    // vector< pair<int, string> > allValidImages;

    // path that includes all the nhdr headers
    string nhdr_path;
    
    // restrict the number of files that we processed
    string maxFileNum;

    // total number of files
    int file_number;

    uint tmax;

    vector< pair<int, string> > allValidFiles;

    // path that includes all the output images
    string out_path;

    // path of the grid file
    string grid_path;

    uint verbose = 0;
};

void setup_resamp(CLI::App &app);

// evaluate 2D convolution between image and kernel
void ConvoEval2D(lspCtx2D* ctx, double xw, double yw);

// evaluate 3D convolution between volume and kernel
void ConvoEval3D(lspCtx3D* ctx, double xw, double yw, double zw);

class Resamp {
    public:
        Resamp(resampOptions const &opt = resampOptions());
        ~Resamp();

        void main();
    
    private:
        resampOptions const opt;
        airArray* mop;
};

#endif //LSP_RESAMP_H