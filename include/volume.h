// The program gives support to various image-related parts
// Created by Zhuokai Zhao
// Contact: zhuokai@uchicago.edu

#ifndef LSP_VOLUME_H
#define LSP_VOLUME_H

#include <stdio.h>
#include <string>
#include <teem/air.h>
#include <teem/biff.h>
#include <teem/hest.h>
#include <teem/nrrd.h>
#include <teem/unrrdu.h>

#include "lsp_math.h"

typedef unsigned int uint;

// volume struct for 3D convolution
typedef struct 
{
    // if non-NULL, a descriptive string of what is in this image, or how it was generated
    char *content;
    
    // how many values are at each pixel; this is always the fastest axis
    uint channel;
    
    // # of samples along three (faster to slower) spatial axes
    uint size[3];
    
    // homogeneous coordinate mapping from index-space (faster coordinate first) to the "right-up" world-space
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
        //unsigned short *us; we don't want unsigned short as data type, would be converted to signed short
        short *s;
        double *dl;
    } data;

} lspVolume;

// lspCtx3D is a container for all the state associated with doing 3D convolution
typedef struct 
{
    int verbose = 0;
    // input
    const lspVolume *volume;
    double volMinMax[3];

    // const NrrdKernel *kern;
    const NrrdKernel *kern;
    // kernel spec
    NrrdKernelSpec* kernelSpec;

    // output fields set by lspConvoEval
    // copy of world-space pos passed to lspConvoEval
    double wpos[3], ipos[3];   

    // convolution result (2 channel)
    double value[2];       

    // homogeneous coordinate mapping from world-space to old volume index space
    double ItoW[16];
    double WtoI[16];

    // boundaries of the region of interests
    uint boundaries[3];

    // check if convolution is inside
    int inside = 1;
    
    // homogeneous coordinate mapping from new volume index space (read from grid.txt)
    double NewItoW[16];

} lspCtx3D;

// initialize a new volume
lspVolume* lspVolumeNew();

// free image structs
lspVolume* lspVolumeNix(lspVolume* vol);

// Allocates a volume. Re-uses an existing data allocation when possible
int lspVolumeAlloc(lspVolume *vol, uint channel, uint size0, uint size1, uint size2, lspType dtype);

// function that loads Volume from Nrrd
int lspVolumeFromNrrd(lspVolume *vol, const Nrrd* nin);

// convert Volume into Nrrd
int lspNrrdFromVolume(Nrrd *nout, const lspVolume *vol);

// lspCtx3DNew creates the context to contain all resources and state
// associated with 3D convolution, which is computing on a given volume "vol" and a reconstruction kernel "kern"
lspCtx3D* lspCtx3DNew(const lspVolume* vol, const std::string gridPath, const NrrdKernel* kernel, const double* vmm, airArray* mop);

// free a lspCtx3D
lspCtx3D* lspCtx3DNix(lspCtx3D* ctx);


#endif //LSP_VOLUME_H
