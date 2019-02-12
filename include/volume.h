// The program gives support to various image-related parts
// Created by Zhuokai Zhao
// Contact: zhuokai@uchicago.edu

#ifndef LSP_VOLUME_H
#define LSP_VOLUME_H

#include <stdio.h>
#include <teem/air.h>
#include <teem/biff.h>
#include <teem/hest.h>
#include <teem/nrrd.h>
#include <teem/unrrdu.h>

#include "lsp_math.h"
// #include "resamp.h"
#include "image.h"

// #ifdef __cplusplus
// extern "C" {
// #endif

typedef unsigned int uint;

// typedef unsigned long long uint64_t;

// volume struct for 3D convolution
typedef struct 
{
    // if non-NULL, a descriptive string of what is in this image, or how it was generated
    char *content;
    // how many values are at each pixel; this always fastest axis
    uint channel;
    // # of samples along faster (size[0]) and slower (size[1]) spatial axes
    uint size[3];
    // homogeneous coordinate mapping from index-space
    // (faster coordinate first) to the "right-up" world-space
    double ItoW[16];
    // type of the data; determines which of the union members below to use
    lspType dtype;
    // union for the pointer to the image data
    // the pointer values are all the same; this is just to avoid casting
    // choose the right union member to use (data.uc vs data.rl) at run-time,
    // according to the value of dtype
    union 
    {
        void *vd;
        unsigned char *uc;
        double *dl;
    } data;

} lspVolume;

// lspCtx3D is a container for all the state associated with doing 3D convolution
typedef struct {
    int verbose = 0;
    // input
    const lspVolume *volume;
    double volMinMax[3];
    // const lspKernel *kern;
    const NrrdKernel *kern;

    // output fields set by lspConvoEval
    // copy of world-space pos passed to lspConvoEval
    double wpos[3], ipos[3];   

    /* If the requested convolution location puts the kernel support
       outside the valid image index domain, "outside" records the
       number of indices missing on the fast axis, plus the number of
       indices missing on the slow axes. */
    uint outside;
    
    // convolution result, if outside == 0
    double value;       
    // double gradient[3];

    /* output set by lspPictureSample: the rate, in kHz, at which output
       pixels could be computed (includes convolution and possibly
       colormapping) */
    double srate;

    // homogeneous coordinate mapping from world-space to index space
    double ItoW[16];
    double WtoI[16];
    // double ItoW_d[4];
} lspCtx3D;

// image.h has this
// double lspNan(unsigned short payload);

// initialize a new volume
lspVolume* lspVolumeNew();

// free image structs
lspVolume* lspVolumeNix(lspVolume* vol);

// lspCtx3DNew creates the context to contain all resources and state
// associated with 3D convolution, which is computing on a given volume "vol", 
// and a reconstruction kernel "kern"
lspCtx3D* lspCtx3DNew(const lspVolume* vol, /*const lspKernel* kern,*/const NrrdKernel* kernel, const double* imm);

// free a lspCtx3D
lspCtx3D* lspCtx3DNix(lspCtx3D* ctx);

// #ifdef __cplusplus
// }
// #endif


#endif //LSP_VOLUME_H
