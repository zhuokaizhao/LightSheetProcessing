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

    // path that includes all the nhdr headers
    string nhdr_path;

    // path of the grid file
    string grid_path;

    // name of the kernel
    string kernel_name;

    // path that includes all the output images
    string out_path;
    
    // restrict the number of files that we processed (coule be empty, which means all files)
    string maxFileNum;

    // the number of files we are processing
    int numFiles;

    // if we are in single file mode
    bool isSingleFile;

    // the index of the file we are currently processing
    int curFileIndex;

    vector< pair<int, string> > allValidFiles;

    uint verbose = 0;
};

void setup_resamp(CLI::App &app);

// function that performs 3D resampling (convolution)
int nrrdResample3D(lspVolume* newVolume, lspCtx3D* ctx3D);

// evaluate 3D convolution between volume and kernel
void convoEval3D(lspCtx3D* ctx, double xw, double yw, double zw, airArray* mop);

class Resamp {
    public:
        Resamp(resampOptions const &opt = resampOptions());
        ~Resamp();

        void main();

        void makeVideo();
    
    private:
        resampOptions const opt;
        airArray* mop;
};

#endif //LSP_RESAMP_H