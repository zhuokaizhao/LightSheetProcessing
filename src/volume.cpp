// The program gives support to various volume-related parts
// Created by Zhuokai Zhao
// Contact: zhuokai@uchicago.edu


#include "volume.h"
#include "lsp_math.h"
// #include "resamp.h"
#include "image.h"
#include <assert.h> // for assert()

// #ifdef __cplusplus
// extern "C" {
// #endif

// identifies this library in biff error messages
const char *lspVolBiffKey = "lspVol";
#define VOL lspVolBiffKey

// utility macro for malloc() and casting to right type
#define MALLOC(N, T) (T*)(malloc((N)*sizeof(T)))

typedef enum 
{
    lspTypeUnknown=0, // (0) (no type known)
    lspTypeUChar,     // (1) 1-byte unsigned char
    lspTypeDouble,    // (2)
} lspType;

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
        biffAddf(VOL, "%s: got NULL pointer", __func__);
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
        biffAddf(VOL, "%s: matrix is not all set: [%g,%g,%g,%g; %g,%g,%g,%g; %g,%g,%g,%g; %g,%g,%g,%g]\n", __func__,
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
        biffAddf(VOL, "%s: ItoW matrix last row [%g,%g,%g,%g] not [0,0,0,1]",
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

/* static because even if this is generally useful;
   it is only needed inside this file */
static int lspNrrdVolumeCheck(const Nrrd *nin) 
{
    if (nrrdCheck(nin)) 
    {
        printf("%s: problem with nrrd itself\n", __func__);
        biffMovef(VOL, NRRD, "%s: problem with nrrd itself", __func__);
        return 1;
    }
    // the type should at least fall into one
    if (!( nrrdTypeUChar == nin->type ||
           nrrdTypeFloat == nin->type ||
           nrrdTypeDouble == nin->type )) 
    {
        printf("%s: can't handle nrrd type %s (need %s, %s, or %s)\n", __func__, airEnumStr(nrrdType, nin->type),
                 airEnumStr(nrrdType, nrrdTypeUChar),
                 airEnumStr(nrrdType, nrrdTypeFloat),
                 airEnumStr(nrrdType, nrrdTypeDouble));
        biffAddf(VOL, "%s: can't handle nrrd type %s (need %s, %s, or %s)",
                 __func__, airEnumStr(nrrdType, nin->type),
                 airEnumStr(nrrdType, nrrdTypeUChar),
                 airEnumStr(nrrdType, nrrdTypeFloat),
                 airEnumStr(nrrdType, nrrdTypeDouble));
        return 1;
    }

    if (!( 3 == nin->dim || 4 == nin->dim )) 
    {
        printf("%s: got dimension %u, not 3 or 4\n", __func__, nin->dim);
        biffAddf(VOL, "%s: got dimension %u, not 3 or 4", __func__, nin->dim);
        return 1;
    }

    uint bidx;
    if (4 == nin->dim) 
    {
        bidx = 1;
        // check the number of channels
        if (!( 1 <= nin->axis[0].size && nin->axis[0].size <= 3 ))
        {
            printf("%s: for 3D array, axis[0] needs size 1, 2, or 3 (not %u)\n", __func__, (uint)(nin->axis[0].size));
            biffAddf(VOL, "%s: for 3D array, axis[0] needs size 1, 2, or 3 (not %u)",
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

    double ItoW[16];

    setItoW3D(ItoW, nin, bidx);

    if (ItoW3DCheck(ItoW)) 
    {
        printf("%s: problem with ItoW\n", __func__);
        biffAddf(VOL, "%s: problem with ItoW", __func__);
        return 1;
    }

    if (!( nrrdCenterCell == nin->axis[bidx+0].center
           && nrrdCenterCell == nin->axis[bidx+1].center )) 
    {
        printf("%s: axis[%u,%u] centering not both %s", __func__,
                 bidx+0, bidx+1, airEnumStr(nrrdCenter, nrrdCenterCell));
        biffAddf(VOL, "%s: axis[%u,%u] centering not both %s", __func__,
                 bidx+0, bidx+1, airEnumStr(nrrdCenter, nrrdCenterCell));
        return 1;
    }
    return 0;
}

static int metaDataCheck(uint channel,
               uint size0, uint size1, uint size2,
               const double *ItoW,
               lspType dtype) 
{
    if (!( 0 < channel && channel <= 3 )) 
    {
        biffAddf(VOL, "%s: invalid channel value %u", __func__, channel);
        return 1;
    }
    if (!( 0 < size0 && 0 < size1 && 0 < size2)) 
    {
        biffAddf(VOL, "%s: invalid volume sizes (%u,%u,%u)",
                 __func__, size0, size1, size2);
        return 1;
    }
    if (airEnumValCheck(lspType_ae, dtype)) 
    {
        biffAddf(VOL, "%s: invalid type %d", __func__, dtype);
        return 1;
    }
    if (ItoW && ItoW3DCheck(ItoW)) 
    { // only call ItoWCheck() on non-NULL ItoW
        biffAddf(VOL, "%s: problem with ItoW matrix", __func__);
        return 1;
    }
    return 0;
}

// *************************** End of static functions ******************

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
  Allocates a volume. Re-uses an existing data allocation when
  possible.  Returns 1 and sets a biff message in case of problems
*/
int lspVolumeAlloc(lspVolume *vol, uint channel,
              uint size0, uint size1, uint size2,
              lspType dtype) 
{
    // some error handlings
    if (!vol) 
    {
        biffAddf(VOL, "%s: got NULL pointer", __func__);
        return 1;
    }

    if (metaDataCheck(channel, size0, size1, size2, NULL, dtype)) 
    {
        biffAddf(VOL, "%s: problem with meta-data", __func__);
        return 1;
    }

    int doalloc;
    if (!(vol->data.vd)) 
    {
        // NULL data; definitely not already allocated
        doalloc = 1;
    } 
    else if (vol->channel != channel
               || vol->size[0] != size0
               || vol->size[1] != size1
               || vol->size[2] != size2
               || vol->dtype != dtype) 
    {
        // already allocated, but not the right size/type
        free(vol->data.vd);
        doalloc = 1;
    } 
    else 
    {
        // cool; re-use existing allocating
        doalloc = 0;
    }
    if (doalloc) 
    {
        vol->data.vd = calloc(channel*size0*size1*size2, typeSize(dtype));
        if (!vol->data.vd) 
        {
            biffAddf(VOL, "%s: failed to allocate %u * %u * %u %s", __func__,
                     channel, size0, size1, airEnumStr(lspType_ae, dtype));
            return 1;
        }
    }

    // img->content untouched
    vol->channel = channel;
    vol->size[0] = size0;
    vol->size[1] = size1;
    vol->size[2] = size2;
    vol->dtype = dtype;
    return 0;
}

// function that loads Volume from Nrrd
int lspVolumeFromNrrd(lspVolume *vol, const Nrrd* nin) 
{
    if (!(vol && nin)) 
    {
        printf("%s: got NULL pointer (%p,%p)\n", __func__, (void*)vol, (void*)nin);
        biffAddf(VOL, "%s: got NULL pointer (%p,%p)\n", __func__,
                 (void*)vol, (void*)nin);
        return 1;
    }

    // check loaded images
    if (lspNrrdVolumeCheck(nin)) 
    {
        printf("%s: given array doesn't conform to a lsp image\n", __func__);
        biffAddf(VOL, "%s: given array doesn't conform to a lsp image", __func__);
        return 1;
    }
    
    uint dim, bidx;
    if (nin->dim == 3) 
    {
        dim = 3;
        bidx = 0;
    } 
    else 
    {
        dim = 4;
        bidx = 1;
    }

    lspType ltype = typeNRRDtoLSP(nin->type);
    if (lspVolumeAlloc(vol, (3 == dim ? nin->axis[0].size : 1),
                      nin->axis[bidx+0].size, nin->axis[bidx+1].size, nin->axis[bidx+2].size,
                      ltype)) 
    {
        printf("%s: trouble allocating volume\n", __func__);
        biffAddf(VOL, "%s: trouble allocating volume", __func__);
        return 1;
    }

    if (nin->content) 
    {
        vol->content = airStrdup(nin->content);
    }

    uint elSize = (uint)nrrdElementSize(nin);
    uint elNum = (uint)nrrdElementNumber(nin);

    if (lspTypeUChar == ltype || nrrdTypeDouble == nin->type) 
    {
        memcpy(vol->data.vd, nin->data, elSize*elNum);
    }
    // else not uchar, so double, and have to check it matches nrrd
    else 
    {
        double (*lup)(const void *, size_t) = nrrdDLookup[nin->type];
        for (uint ii=0; ii<elNum; ii++) 
        {
            vol->data.dl[ii] = lup(nin->data, ii);
        }
    }

    setItoW3D(vol->ItoW, nin, bidx);

    return 0;
}

// wrapping image into Nrrd
int lspVolumeNrrdWrap(Nrrd *nout, const lspVolume *vol) 
{
    if (!(nout && vol)) 
    {
        biffAddf(VOL, "%s: got NULL pointer (%p,%p)\n", __func__,
                 (void*)nout, (void*)vol);
        return 1;
    }

    uint ndim, bidx;
    size_t size[3];
    
    if (1 == vol->channel) 
    {
        ndim = 3;
        bidx = 0;
    } 
    else 
    {
        ndim = 4;
        size[0] = vol->channel;
        bidx = 1;
    }

    size[bidx+0] = vol->size[1];
    size[bidx+1] = vol->size[2];
    size[bidx+2] = vol->size[3];
    
    if (nrrdWrap_nva(nout, vol->data.vd, typeNRRDtoLSP(vol->dtype),
                     ndim, size)) 
    {
        biffMovef(VOL, NRRD, "%s: trouble wrapping data in nrrd", __func__);
        return 1;
    }

    if (vol->content) 
    {
        nout->content = airStrdup(vol->content);
    }

    nrrdSpaceSet(nout, nrrdSpaceRightUp);
    /* 0   1  2  3
       4   5  6  7
       8   9 10 11
       12 13 14 15 */

    // direction
    // x-direction
    nout->axis[bidx+0].spaceDirection[0] = vol->ItoW[0];
    nout->axis[bidx+0].spaceDirection[1] = vol->ItoW[4];
    nout->axis[bidx+0].spaceDirection[2] = vol->ItoW[8];
    // y-direction
    nout->axis[bidx+1].spaceDirection[0] = vol->ItoW[1];
    nout->axis[bidx+1].spaceDirection[1] = vol->ItoW[5];
    nout->axis[bidx+1].spaceDirection[2] = vol->ItoW[9];
    // z-direction
    nout->axis[bidx+2].spaceDirection[0] = vol->ItoW[2];
    nout->axis[bidx+2].spaceDirection[1] = vol->ItoW[6];
    nout->axis[bidx+2].spaceDirection[2] = vol->ItoW[10];

    // origin
    nout->spaceOrigin[0] = vol->ItoW[3];
    nout->spaceOrigin[1] = vol->ItoW[7];
    nout->spaceOrigin[2] = vol->ItoW[11];
    
    // sampling method
    nout->axis[bidx+0].center = nrrdCenterCell;
    nout->axis[bidx+1].center = nrrdCenterCell;
    nout->axis[bidx+2].center = nrrdCenterCell;

    return 0;
}

/*
  lspCtx3DNew creates the context to contain all resources and state
  associated with 2D convolution, which is computing on a given volume "vol", 
  and a reconstruction kernel "kernel"
*/
lspCtx3D* lspCtx3DNew(const lspVolume* vol, /*const lspKernel* kern,*/const NrrdKernel* kernel, const double* vmm) 
{
    // some error checks
    if (!(vol && kernel)) 
    {
        biffAddf(VOL, "%s: got NULL pointer (%p,%p)", __func__,
                 (void*)vol, (void*)kernel);
        return NULL;
    }
    if (1 != vol->channel) 
    {
        biffAddf(VOL, "%s: only works on scalar (not %u-channel) images",
                 __func__, vol->channel);
        return NULL;
    }
    
    lspCtx3D *ctx = MALLOC(1, lspCtx3D);
    assert(ctx);
    ctx->verbose = 0;
    ctx->volume = vol;
    ctx->kern = kernel;
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

// free a lspCtx3D
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