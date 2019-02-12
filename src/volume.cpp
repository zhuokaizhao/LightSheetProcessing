// The program gives support to various volume-related parts
// Created by Zhuokai Zhao
// Contact: zhuokai@uchicago.edu


#include "volume.h"
#include "lsp_math.h"
#include <assert.h> // for assert()

// #ifdef __cplusplus
// extern "C" {
// #endif

// identifies this library in biff error messages
const char *lspBiffKey = "lsp";
#define LSP lspBiffKey

// utility macro for malloc() and casting to right type
#define MALLOC(N, T) (T*)(malloc((N)*sizeof(T)))

/* an airEnum is a gadget for managing identifications between
   integers (C enum values) and strings */
static const airEnum _lspType_ae = {
    "pixel value type",
    2,
    (const char*[]) { "(unknown_type)", "uchar",      "double" },
    (int [])        {lspTypeUnknown,    lspTypeUChar, lspTypeDouble},
    (const char*[]) {
        "unknown type",
        "unsigned char",
        "double"
    },
    NULL, NULL,
    AIR_FALSE
};
const airEnum *const lspType_ae = &_lspType_ae;


// *********************** static functions start **********************

// convert from nrrd type to lsptype
static lspType typeNRRDtoLSP(int ntype) 
{
    lspType ret;
    
    switch (ntype) 
    {
        case nrrdTypeUChar: ret = lspTypeUChar;
            break;
        case nrrdTypeDouble: // one of these is nrrdTypeDouble
            ret = lspTypeDouble;
            break;
        default: ret = lspTypeUnknown;
    }

    return ret;
}

// set values as 3x3 ItoW matrix
static void setItoW3D(double *ItoW, const Nrrd *nin, uint bidx) 
{
    /* 0  1  2  3
       4  5  6  7
       8  9  10 11
       12 13 14 15 */

    // first column
    ItoW[0] = (double)(nin->axis[bidx+0].spaceDirection[0]);
    ItoW[4] = (double)(nin->axis[bidx+0].spaceDirection[1]);
    ItoW[8] = (double)(nin->axis[bidx+0].spaceDirection[2]);
    ItoW[12] = 0;

    // second column
    ItoW[1] = (double)(nin->axis[bidx+1].spaceDirection[0]);
    ItoW[5] = (double)(nin->axis[bidx+1].spaceDirection[1]);
    ItoW[9] = (double)(nin->axis[bidx+1].spaceDirection[2]);
    ItoW[13] = 0;

    // third column
    ItoW[2] = (double)(nin->axis[bidx+2].spaceDirection[0]);
    ItoW[6] = (double)(nin->axis[bidx+2].spaceDirection[1]);
    ItoW[10] = (double)(nin->axis[bidx+2].spaceDirection[2]);
    ItoW[14] = 0;

    // fourth column
    ItoW[3] = (double)(nin->spaceOrigin[0]);
    ItoW[7] = (double)(nin->spaceOrigin[1]);
    ItoW[11] = (double)(nin->spaceOrigin[2]);
    ItoW[15] = 1;
}

// check if a ItoW matrix is valid
static int ItoW3DCheck(const double *ItoW) 
{
    if (!ItoW) 
    {
        printf("%s: got NULL pointer\n", __func__);
        biffAddf(LSP, "%s: got NULL pointer", __func__);
        return 1;
    }
    
    // check if entries are finite
    if (!M4_ISFINITE(ItoW)) 
    {
        printf("%s: matrix is not all set: [%g,%g,%g,%g; %g,%g,%g,%g; %g,%g,%g,%g; %g,%g,%g,%g]\n", __func__,
                 ItoW[0], ItoW[1], ItoW[2], ItoW[3],
                 ItoW[4], ItoW[5], ItoW[6], ItoW[7],
                 ItoW[8], ItoW[9], ItoW[10], ItoW[11],
                 ItoW[12], ItoW[13], ItoW[14], ItoW[15]);
        biffAddf(LSP, "%s: matrix is not all set: [%g,%g,%g,%g; %g,%g,%g,%g; %g,%g,%g,%g; %g,%g,%g,%g]\n", __func__,
                 ItoW[0], ItoW[1], ItoW[2], ItoW[3],
                 ItoW[4], ItoW[5], ItoW[6], ItoW[7],
                 ItoW[8], ItoW[9], ItoW[10], ItoW[11],
                 ItoW[12], ItoW[13], ItoW[14], ItoW[15]);
        return 1;
    }

    // check if the last row is 0 0 1
    if (!( 0 == ItoW[12] &&
           0 == ItoW[13] &&
           0 == ItoW[14] &&
           1 == ItoW[15] )) 
    {
        printf("%s: ItoW matrix last row [%g,%g,%g, %g] not [0,0,0,1]\n", __func__, ItoW[12], ItoW[13], ItoW[14], ItoW[15]);
        biffAddf(LSP, "%s: ItoW matrix last row [%g,%g,%g,%g] not [0,0,0,1]",
                 __func__, ItoW[12], ItoW[13], ItoW[14], ItoW[15]);
        return 1;
    }

    // else no problems
    return 0;
}

// get the size of a type
static size_t typeSize(lspType dtype) 
{
    size_t ret;
    switch (dtype) 
    {
        case lspTypeUChar: ret = sizeof(unsigned char);
            break;
        case lspTypeDouble: ret = sizeof(double);
            break;
        default: ret = 0;
    }
    return ret;
}

// *************************** End of static functions ******************

double lspNan(unsigned short payload)
{
    double rr;
    /* the logic for both cases is the same: make it a non-finite number by
       setting all the exponent bits, make it a NaN by making sure the highest
       bit of the fraction is on (else it would be an infinity), and then put
       the 16-bit payload in the highest bits (because experience suggests
       these will more likely survive a conversion between a 32-bit float and
       64-bit double */
    //                                                     52 - 1 - 16 == 35
    rr = ((unsigned long long)0x7ff<<52)| ((unsigned long long)1<<51) | ((unsigned long long)payload<<35);
    return rr;
}

// initialize a new volume
lspVolume* lspVolumeNew() 
{
    lspVolume *ret = MALLOC(1, lspVolume);
    assert(ret);
    ret->content = NULL;
    ret->channel = ret->size[0] = ret->size[1] = 0;
    M4_SET_NAN(ret->ItoW);
    ret->dtype = lspTypeUnknown;
    ret->data.vd = NULL;
    return ret;
}

// free volume structs
lspVolume* lspVolumeNix(lspVolume* vol) 
{
    if (vol) 
    {
        if (vol->content) 
        {
            free(vol->content);
        }
        
        free(vol->data.vd);
        free(vol);
    }

    return NULL;
}

/*
  lspCtx2DNew creates the context to contain all resources and state
  associated with 2D convolution, which is computing on a given image "img", 
  and a reconstruction kernel "kern"
*/
lspCtx3D* lspCtx3DNew(const lspVolume* vol, const lspKernel* kern, const double* vmm) 
{
    // some error checks
    if (!(vol && kern)) 
    {
        biffAddf(LSP, "%s: got NULL pointer (%p,%p)", __func__,
                 (void*)vol, (void*)kern);
        return NULL;
    }
    if (1 != vol->channel) 
    {
        biffAddf(LSP, "%s: only works on scalar (not %u-channel) images",
                 __func__, vol->channel);
        return NULL;
    }
    
    lspCtx3D *ctx = MALLOC(1, lspCtx3D);
    assert(ctx);
    ctx->verbose = 0;
    ctx->volume = vol;
    ctx->kern = kern;
    ctx->volMinMax[0] = vmm ? vmm[0] : lspNan(0);
    ctx->volMinMax[1] = vmm ? vmm[1] : lspNan(0);
    ctx->volMinMax[2] = vmm ? vmm[2] : lspNan(0);


    // copy image->ItoW for consistancy
    M4_COPY(ctx->ItoW, ctx->volume->ItoW);
    // convert wpos to index-space
    // sets 4x4 WtoI to inverse of 4x4 ctx->image->ItoW, using tmp variable TMP
    double TMP;
    M4_INVERSE(ctx->WtoI, ctx->volume->ItoW, TMP);

    // change gradient sum from index to world space (Equation 4.82 in FSV)
    //M23_INVERSE_TRANSPOSE(ctx->ItoW_d, ctx->image->ItoW, TMP);

    return ctx;
}

// free a lspCtx2D
lspCtx3D* lspCtx3DNix(lspCtx3D* ctx)
{

    if (ctx) 
    {
        free(ctx);
    }
    return NULL;
}

// #ifdef __cplusplus
// }
// #endif