// The program performs resampling process on images genereated by anim.cpp
// Created by Zhuokai Zhao
// Contact: zhuokai@uchicago.edu


#ifndef LSP_RESAMP_H
#define LSP_RESAMP_H

#include <vector>
#include "CLI11.hpp"
#include "lsp_math.h"

using namespace std;

struct resampOptions {
    // path that includes all the input images obtained from anim
    string image_path;
    // restrict the number of files that we processed
    string maxFileNum;
    // path that includes all the output images
    string out_path;
    // path of the kernel file
    string kernel_path;
    
    uint verbose = 0;
}

// mprCtx is a container for all the state associated with doing convolution
typedef struct {
    // input
    const mprImage *image;
    const mprKernel *kern;

    // output fields set by mprConvoEval
    // copy of world-space pos passed to mprConvoEval
    double wpos[2], ipos[2];   

    /* If the requested convolution location puts the kernel support
       outside the valid image index domain, "outside" records the
       number of indices missing on the fast axis, plus the number of
       indices missing on the slow axes. */
    uint outside;
    
    // convolution result, if outside == 0
    double value;       
    double gradient[2];

    /* output set by mprPictureSample: the rate, in kHz, at which output
       pixels could be computed (includes convolution and possibly
       colormapping) */
    double srate;

    // homogeneous coordinate mapping from world-space to index space
    real ItoW[9];
    real WtoI[9];
    real ItoW_d[4];
} mprCtx;

void ConvoEval(mprCtx *ctx, real xw, real yw);