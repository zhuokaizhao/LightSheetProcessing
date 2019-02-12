// The program gives support to various image-related parts
// Created by Zhuokai Zhao
// Contact: zhuokai@uchicago.edu

#ifndef LSP_IMAGE_H
#define LSP_IMAGE_H

#include <stdio.h>
#include <teem/air.h>
#include <teem/biff.h>
#include <teem/hest.h>
#include <teem/nrrd.h>
#include <teem/unrrdu.h>

#include "lsp_math.h"

// #ifdef __cplusplus
// extern "C" {
// #endif

typedef unsigned int uint;


// typedef unsigned long long uint64_t;

/*
  lspType: the scalar pixel types supported in lspImage
  lspTypeUChar is only used for output images.
  All input images (and some outputs) are lspTypeReal.
*/
typedef enum 
{
    lspTypeUnknown=0, // (0) (no type known)
    lspTypeUChar,     // (1) 1-byte unsigned char
    lspTypeDouble,    // (2)
} lspType;

// image struct for 2D convolution
typedef struct 
{
    // if non-NULL, a descriptive string of what is in this image, or how it was generated
    char *content;
    // how many values are at each pixel; this always fastest axis
    uint channel;
    // # of samples along faster (size[0]) and slower (size[1]) spatial axes
    uint size[2];
    // homogeneous coordinate mapping from index-space
    // (faster coordinate first) to the "right-up" world-space
    double ItoW[9];
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

} lspImage;

/* The lspKernel stores everything about a reconstruction kernel. The kernel
   is non-zero only within [-support/2,support/2], for integer "support"
   which may be odd or even (but always positive). The kernels are set up at
   compile-time in such a way that each kernel knows its own derivative; the
   derivative of lspKernel *k is k->deriv. */
typedef struct lspKernel_t 
{
    // short identifying string
    const char *name;
    // short descriptive string
    const char *desc;
    // # samples needed for convolution
    unsigned int support;             
    // evaluate kernel once
    double (*eval)(double xx);
    /* The "apply" function evaluates the kernel "support" times, saving the
       results into ww, as needed for one convolution (FSV section 4.2.2).
       If the kernel support is even, then xa must satisfy 0 <= xa < 1,
       and let ilo = 1 - support/2, see FSV (4.32).
       If support is odd, then xa must satisfy -0.5 <= xa < 0.5,
       and let ilo = (1 - support)/2, see FSV (4.36).
       Then, ww[i] = eval(xa-(i+ilo)) for i=0, 1, ... support-1.
       This may be faster than calling eval() "support" times. */
    void (*apply)(double *ww, double xa);
    // derivative of this kernel; will point back to itself when kernel is zero
    const struct lspKernel_t *deriv;
} lspKernel;

// lspCtx2D is a container for all the state associated with doing 2D convolution
typedef struct {
    int verbose = 0;
    // input
    const lspImage *image;
    double imgMinMax[2];
    const lspKernel *kern;

    // output fields set by lspConvoEval
    // copy of world-space pos passed to lspConvoEval
    double wpos[2], ipos[2];   

    /* If the requested convolution location puts the kernel support
       outside the valid image index domain, "outside" records the
       number of indices missing on the fast axis, plus the number of
       indices missing on the slow axes. */
    uint outside;
    
    // convolution result, if outside == 0
    double value;       
    // double gradient[2];

    /* output set by lspPictureSample: the rate, in kHz, at which output
       pixels could be computed (includes convolution and possibly
       colormapping) */
    double srate;

    // homogeneous coordinate mapping from world-space to index space
    double ItoW[9];
    double WtoI[9];
    // double ItoW_d[4];
} lspCtx2D;

double lspNan(unsigned short payload);

// initialize a new image
lspImage* lspImageNew();

// free image structs
lspImage* lspImageNix(lspImage* img);

// lspCtx2DNew creates the context to contain all resources and state
// associated with 2D convolution, which is computing on a given image "img", 
// and a reconstruction kernel "kern"
lspCtx2D* lspCtx2DNew(const lspImage* img, const lspKernel* kern, const double* imm);

// free a lspCtx
lspCtx2D* lspCtx2DNix(lspCtx2D* ctx);

// Allocates an image. Re-uses an existing data allocation when
// possible.  Returns 1 and sets a biff message in case of problems
int lspImageAlloc(lspImage *img, uint channel, uint size0, uint size1, lspType dtype);

// load image
int lspImageLoad(lspImage *img, const char *fname);

// wraping image to nrrd
int lspImageNrrdWrap(Nrrd *nout, const lspImage *img);
/*
  lspImageMinMax discovers the range of finite values in a given image,
  with some error handling. Will always set minmax[0,1] to some
  finite range, even if the input was all non-finite values.
*/
int lspImageMinMax(double minmax[2], int *allFinite, const lspImage *img);

// save image to fname
int lspImageSave(const char *fname, const lspImage *img);

// #ifdef __cplusplus
// }
// #endif


#endif //LSP_IMAGE_H
