// The program gives support to various image-related parts
// Created by Zhuokai Zhao
// Contact: zhuokai@uchicago.edu


#include "image.h"
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
static void setItoW(double *ItoW, const Nrrd *nin, uint bidx) 
{
    /* 0  1  2
       3  4  5
       6  7  8 */
    ItoW[0] = (double)(nin->axis[bidx+0].spaceDirection[0]);
    ItoW[3] = (double)(nin->axis[bidx+0].spaceDirection[1]);
    ItoW[6] = 0;
    ItoW[1] = (double)(nin->axis[bidx+1].spaceDirection[0]);
    ItoW[4] = (double)(nin->axis[bidx+1].spaceDirection[1]);
    ItoW[7] = 0;
    ItoW[2] = (double)(nin->spaceOrigin[0]);
    ItoW[5] = (double)(nin->spaceOrigin[1]);
    ItoW[8] = 1;
}

// check if a ItoW matrix is valid
static int ItoWCheck(const double *ItoW) 
{
    if (!ItoW) 
    {
        biffAddf(LSP, "%s: got NULL pointer", __func__);
        return 1;
    }
    
    // check if entries are finite
    if (!M3_ISFINITE(ItoW)) 
    {
        biffAddf(LSP, "%s: matrix is not all set: "
                 "[%g,%g,%g; %g,%g,%g; %g,%g,%g]", __func__,
                 ItoW[0], ItoW[1], ItoW[2],
                 ItoW[3], ItoW[4], ItoW[5],
                 ItoW[6], ItoW[7], ItoW[8]);
        return 1;
    }

    // check if the last row is 0 0 1
    if (!( 0 == ItoW[6] &&
           0 == ItoW[7] &&
           1 == ItoW[8] )) 
    {
        biffAddf(LSP, "%s: ItoW matrix last row [%g,%g,%g] not [0,0,1]",
                 __func__, ItoW[6], ItoW[7], ItoW[8]);
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

/* static because even if this is generally useful;
   it is only needed inside this file */
static int lspNrrdImageCheck(const Nrrd *nin) 
{
    if (nrrdCheck(nin)) 
    {
        printf("%s: problem with nrrd itself\n", __func__);
        biffMovef(LSP, NRRD, "%s: problem with nrrd itself", __func__);
        return 1;
    }
    if (!( nrrdTypeUChar == nin->type ||
           nrrdTypeFloat == nin->type ||
           nrrdTypeDouble == nin->type )) 
    {
        printf("%s: can't handle nrrd type %s (need %s, %s, or %s)\n", __func__, airEnumStr(nrrdType, nin->type),
                 airEnumStr(nrrdType, nrrdTypeUChar),
                 airEnumStr(nrrdType, nrrdTypeFloat),
                 airEnumStr(nrrdType, nrrdTypeDouble));
        biffAddf(LSP, "%s: can't handle nrrd type %s (need %s, %s, or %s)",
                 __func__, airEnumStr(nrrdType, nin->type),
                 airEnumStr(nrrdType, nrrdTypeUChar),
                 airEnumStr(nrrdType, nrrdTypeFloat),
                 airEnumStr(nrrdType, nrrdTypeDouble));
        return 1;
    }

    if (!( 2 == nin->dim || 3 == nin->dim )) 
    {
        printf("%s: got dimension %u, not 2 or 3\n", __func__, nin->dim);
        biffAddf(LSP, "%s: got dimension %u, not 2 or 3", __func__, nin->dim);
        return 1;
    }

    uint bidx;
    if (3 == nin->dim) 
    {
        bidx = 1;
        if (!( 1 <= nin->axis[0].size && nin->axis[0].size <= 3 ))
        {
            printf("%s: for 3D array, axis[0] needs size 1, 2, or 3 (not %u)\n", __func__, (uint)(nin->axis[0].size));
            biffAddf(LSP, "%s: for 3D array, axis[0] needs size 1, 2, or 3 (not %u)",
                     __func__, (uint)(nin->axis[0].size));
            return 1;
        }
    } 
    else 
    {
        bidx = 0;
    }
    
    // for some reasons this cannot be passed
    // if (airEnumValCheck(nrrdSpace, nin->space)) 
    // {
    //     printf("%s: array space %u not set or known\n", __func__, nin->space);
    //     biffAddf(LSP, "%s: array space %u not set or known", __func__, nin->space);
    //     return 1;
    // }

    // if (nin->space != nrrdSpaceRightUp) 
    // {
    //     printf("%s: array space %s not expected %s\n", __func__,
    //              airEnumStr(nrrdSpace, nin->space),
    //              airEnumStr(nrrdSpace, nrrdSpaceRightUp));
    //     biffAddf(LSP, "%s: array space %s not expected %s", __func__,
    //              airEnumStr(nrrdSpace, nin->space),
    //              airEnumStr(nrrdSpace, nrrdSpaceRightUp));
    //     return 1;
    // }

    double ItoW[9];

    setItoW(ItoW, nin, bidx);

    if (ItoWCheck(ItoW)) 
    {
        printf("%s: problem with ItoW", __func__);
        biffAddf(LSP, "%s: problem with ItoW", __func__);
        return 1;
    }

    if (!( nrrdCenterCell == nin->axis[bidx+0].center
           && nrrdCenterCell == nin->axis[bidx+1].center )) 
    {
        printf("%s: axis[%u,%u] centering not both %s", __func__,
                 bidx+0, bidx+1, airEnumStr(nrrdCenter, nrrdCenterCell));
        biffAddf(LSP, "%s: axis[%u,%u] centering not both %s", __func__,
                 bidx+0, bidx+1, airEnumStr(nrrdCenter, nrrdCenterCell));
        return 1;
    }
    return 0;
}

static int metaDataCheck(uint channel,
               uint size0, uint size1,
               const double *ItoW,
               lspType dtype) 
{
    if (!( 0 < channel && channel <= 3 )) 
    {
        biffAddf(LSP, "%s: invalid channel value %u", __func__, channel);
        return 1;
    }
    if (!( 0 < size0 && 0 < size1 )) 
    {
        biffAddf(LSP, "%s: invalid image sizes (%u,%u)",
                 __func__, size0, size1);
        return 1;
    }
    if (airEnumValCheck(lspType_ae, dtype)) 
    {
        biffAddf(LSP, "%s: invalid type %d", __func__, dtype);
        return 1;
    }
    if (ItoW && ItoWCheck(ItoW)) 
    { // only call ItoWCheck() on non-NULL ItoW
        biffAddf(LSP, "%s: problem with ItoW matrix", __func__);
        return 1;
    }
    return 0;
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

// initialize a new image
lspImage* lspImageNew() 
{
    lspImage *ret = MALLOC(1, lspImage);
    assert(ret);
    ret->content = NULL;
    ret->channel = ret->size[0] = ret->size[1] = 0;
    M3_SET_NAN(ret->ItoW);
    ret->dtype = lspTypeUnknown;
    ret->data.vd = NULL;
    return ret;
}

// free image structs
lspImage* lspImageNix(lspImage* img) 
{
    if (img) 
    {
        if (img->content) 
        {
            free(img->content);
        }
        
        free(img->data.vd);
        free(img);
    }

    return NULL;
}

/*
  lspCtxNew creates the context to contain all resources and state
  associated with computing on a given image "img", reconstruction kernel
  "kern"
*/
lspCtx* lspCtxNew(const lspImage* img, const lspKernel* kern, const double* imm) 
{
    // some error checks
    if (!(img && kern)) 
    {
        biffAddf(LSP, "%s: got NULL pointer (%p,%p)", __func__,
                 (void*)img, (void*)kern);
        return NULL;
    }
    if (1 != img->channel) 
    {
        biffAddf(LSP, "%s: only works on scalar (not %u-channel) images",
                 __func__, img->channel);
        return NULL;
    }
    
    lspCtx *ctx = MALLOC(1, lspCtx);
    assert(ctx);
    ctx->verbose = 0;
    ctx->image = img;
    ctx->kern = kern;
    ctx->imgMinMax[0] = imm ? imm[0] : lspNan(0);
    ctx->imgMinMax[1] = imm ? imm[1] : lspNan(0);

    // copy image->ItoW for consistancy
    M3_COPY(ctx->ItoW, ctx->image->ItoW);
    // convert wpos to index-space
    // sets 3x3 WtoI to inverse of 3x3 ctx->image->ItoW, using tmp variable TMP
    double TMP;
    M3_INVERSE(ctx->WtoI, ctx->image->ItoW, TMP);

    // change gradient sum from index to world space (Equation 4.82 in FSV)
    //M23_INVERSE_TRANSPOSE(ctx->ItoW_d, ctx->image->ItoW, TMP);

    return ctx;
}

// free a lspCtx
lspCtx* lspCtxNix(lspCtx* ctx)
{

    if (ctx) 
    {
        free(ctx);
    }
    return NULL;
}

/*
  Allocates an image. Re-uses an existing data allocation when
  possible.  Returns 1 and sets a biff message in case of problems
*/
int lspImageAlloc(lspImage *img, uint channel,
              uint size0, uint size1,
              lspType dtype) 
{
    // some error handlings
    if (!img) 
    {
        biffAddf(LSP, "%s: got NULL pointer", __func__);
        return 1;
    }

    if (metaDataCheck(channel, size0, size1, NULL, dtype)) 
    {
        biffAddf(LSP, "%s: problem with meta-data", __func__);
        return 1;
    }

    int doalloc;
    if (!(img->data.vd)) 
    {
        // NULL data; definitely not already allocated
        doalloc = 1;
    } 
    else if (img->channel != channel
               || img->size[0] != size0
               || img->size[1] != size1
               || img->dtype != dtype) 
    {
        // already allocated, but not the right size/type
        free(img->data.vd);
        doalloc = 1;
    } 
    else 
    {
        // cool; re-use existing allocating
        doalloc = 0;
    }
    if (doalloc) 
    {
        img->data.vd = calloc(channel*size0*size1, typeSize(dtype));
        if (!img->data.vd) 
        {
            biffAddf(LSP, "%s: failed to allocate %u * %u * %u %s", __func__,
                     channel, size0, size1, airEnumStr(lspType_ae, dtype));
            return 1;
        }
    }

    // img->content untouched
    img->channel = channel;
    img->size[0] = size0;
    img->size[1] = size1;
    img->dtype = dtype;
    return 0;
}

// function that loads images
int lspImageLoad(lspImage *img, const char *fname) 
{
    if (!(img && fname)) 
    {
        printf("%s: got NULL pointer (%p,%p)\n", __func__, (void*)img, (void*)fname);
        biffAddf(LSP, "%s: got NULL pointer (%p,%p)\n", __func__,
                 (void*)img, (void*)fname);
        return 1;
    }

    airArray *mop = airMopNew();
    Nrrd *nin = nrrdNew();
    airMopAdd(mop, nin, (airMopper)nrrdNuke, airMopAlways);

    // load images
    if (nrrdLoad(nin, fname, NULL)) 
    {
        printf("%s: trouble reading file\n", __func__);
        biffMovef(LSP, NRRD, "%s: trouble reading file", __func__);
        airMopError(mop);
        return 1;
    }

    // check loaded images
    if (lspNrrdImageCheck(nin)) 
    {
        printf("%s: given array doesn't conform to a lsp image\n", __func__);
        biffAddf(LSP, "%s: given array doesn't conform to a lsp image", __func__);
        airMopError(mop);
        return 1;
    }
    
    uint dim, bidx;
    if (nin->dim == 2) 
    {
        dim = 2;
        bidx = 0;
    } 
    else 
    {
        dim = 3;
        bidx = 1;
    }

    lspType mtype = typeNRRDtoLSP(nin->type);
    if (lspImageAlloc(img, (3 == dim ? nin->axis[0].size : 1),
                      nin->axis[bidx+0].size, nin->axis[bidx+1].size,
                      mtype)) 
    {
        printf("%s: trouble allocating image\n", __func__);
        biffAddf(LSP, "%s: trouble allocating image", __func__);
        airMopError(mop);
        return 1;
    }

    if (nin->content) 
    {
        img->content = airStrdup(nin->content);
    }

    uint elSize = (uint)nrrdElementSize(nin);
    uint elNum = (uint)nrrdElementNumber(nin);

    if (lspTypeUChar == mtype || nrrdTypeDouble == nin->type) 
    {
        memcpy(img->data.vd, nin->data, elSize*elNum);
    }
    // else not uchar, so double, and have to check it matches nrrd
    else 
    {
        double (*lup)(const void *, size_t) = nrrdDLookup[nin->type];
        for (uint ii=0; ii<elNum; ii++) 
        {
            img->data.dl[ii] = lup(nin->data, ii);
        }
    }

    setItoW(img->ItoW, nin, bidx);

    airMopOkay(mop);
    return 0;
}

// wrapping image into Nrrd
int lspImageNrrdWrap(Nrrd *nout, const lspImage *img) 
{
    if (!(nout && img)) 
    {
        biffAddf(LSP, "%s: got NULL pointer (%p,%p)\n", __func__,
                 (void*)nout, (void*)img);
        return 1;
    }

    uint ndim, bidx;
    size_t size[3];
    
    if (1 == img->channel) 
    {
        ndim = 2;
        bidx = 0;
    } 
    else 
    {
        ndim = 3;
        size[0] = img->channel;
        bidx = 1;
    }

    size[bidx+0] = img->size[0];
    size[bidx+1] = img->size[1];
    
    if (nrrdWrap_nva(nout, img->data.vd, typeNRRDtoLSP(img->dtype),
                     ndim, size)) 
    {
        biffMovef(LSP, NRRD, "%s: trouble wrapping data in nrrd", __func__);
        return 1;
    }

    if (img->content) 
    {
        nout->content = airStrdup(img->content);
    }

    nrrdSpaceSet(nout, nrrdSpaceRightUp);
    /* 0  1  2
       3  4  5
       6  7  8 */
    nout->axis[bidx+0].spaceDirection[0] = img->ItoW[0];
    nout->axis[bidx+0].spaceDirection[1] = img->ItoW[3];
    nout->axis[bidx+1].spaceDirection[0] = img->ItoW[1];
    nout->axis[bidx+1].spaceDirection[1] = img->ItoW[4];
    nout->spaceOrigin[0] = img->ItoW[2];
    nout->spaceOrigin[1] = img->ItoW[5];
    nout->axis[bidx+0].center = nrrdCenterCell;
    nout->axis[bidx+1].center = nrrdCenterCell;

    return 0;
}

/*
  lspImageMinMax discovers the range of finite values in a given image,
  with some error handling. Will always set minmax[0,1] to some
  finite range, even if the input was all non-finite values.
*/
int lspImageMinMax(double minmax[2], int *allFinite, const lspImage *img) 
{
    // check if input and output pointers are valid
    if (!(minmax && img)) 
    {
        // allFinite can be NULL
        biffAddf(LSP, "%s: got NULL pointer (%p,%p)", __func__,
                 (void*)minmax, (void*)img);
        return 1;
    }

    // allocate
    airArray *mop = airMopNew();
    Nrrd *nin = nrrdNew();
    airMopAdd(mop, nin, (airMopper)nrrdNix, airMopAlways);

    // convert from image to nrrd
    if (lspImageNrrdWrap(nin, img)) 
    {
        biffAddf(LSP, "%s: trouble wrapping image as nrrd", __func__);
        airMopError(mop); return 1;
    }

    // obtain range from nrrd file
    NrrdRange *range = nrrdRangeNew(AIR_NAN, AIR_NAN);
    airMopAdd(mop, range, (airMopper)nrrdRangeNix, airMopAlways);
    nrrdRangeSet(range, nin, AIR_FALSE);

    if (allFinite) 
    {
        *allFinite = (nrrdHasNonExistFalse == range->hasNonExist);
    }

    // assign output values
    if (!( isfinite(range->min) && isfinite(range->max) )) 
    {
        // *all* inputs were non-finite, so invent a range
        minmax[0] = 0;
        minmax[1] = 1;
    } 
    else 
    {
        // record discovered finite range
        minmax[0] = range->min;
        minmax[1] = range->max;
    }

    airMopOkay(mop);

    return 0;
}

// save the image
int lspImageSave(const char *fname, const lspImage *img) 
{
    if (!(fname && img)) 
    {
        biffAddf(LSP, "%s: got NULL pointer (%p,%p)\n", __func__,
                 (void*)fname, (void*)img);
        return 1;
    }
    airArray *mop = airMopNew();
    Nrrd *nout = nrrdNew();
    airMopAdd(mop, nout, (airMopper)nrrdNix, airMopAlways);
    
    if (lspImageNrrdWrap(nout, img)) 
    {
        biffAddf(LSP, "%s: trouble wrapping image into Nrrd", __func__);
        airMopError(mop);
        return 1;
    }
    
    if (nrrdSave(fname, nout, NULL)) 
    {
        biffMovef(LSP, NRRD, "%s: trouble saving output", __func__);
        airMopError(mop);
        return 1;
    }

    airMopOkay(mop);
    return 0;
}

// #ifdef __cplusplus
// }
// #endif