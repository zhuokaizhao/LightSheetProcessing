// The program gives support to various volume-related parts
// Created by Zhuokai Zhao
// Contact: zhuokai@uchicago.edu


#include "volume.h"
#include "lsp_math.h"
#include "image.h"
#include <assert.h> // for assert()
#include <string>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <vector>
#include <sstream>

using namespace std;

// identifies this library in biff error messages
const char *lspVolBiffKey = "lspVol";
#define VOL lspVolBiffKey

// utility macro for malloc() and casting to right type
#define MALLOC(N, T) (T*)(malloc((N)*sizeof(T)))

/* an airEnum is a gadget for managing identifications between
   integers (C enum values) and strings */
static const airEnum _lspType_ae = 
{
    "pixel value type", //name
    4, // number of valid types
    (const char*[]) { "(unknown_type)", "uchar",      "short",      "unsigned short",   "double" },
    (int [])        {lspTypeUnknown,    lspTypeUChar, lspTypeShort, lspTypeUShort,      lspTypeDouble},
    (const char*[]) {
        "unknown type",
        "unsigned char",
        "short",
        "unsigned short",
        "double"
    },
    NULL, NULL,
    AIR_FALSE // if require case matching on strings
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
        case nrrdTypeShort: ret = lspTypeShort;
            break;
        case nrrdTypeUShort: ret = lspTypeUShort;
            break;
        case nrrdTypeDouble: ret = lspTypeDouble;
            break;
        default: 
            ret = lspTypeUnknown;
    }

    return ret;
}

// convert from lsptype to nrrd type
static int typeLSPtoNRRD(lspType ltype) 
{
    int ret;
    
    switch (ltype) 
    {
        case lspTypeUChar: ret = nrrdTypeUChar;
            break;
        case lspTypeShort: ret = nrrdTypeShort;
            break;
        case lspTypeUShort: ret = nrrdTypeUShort;
            break;
        case lspTypeDouble: ret = nrrdTypeDouble;
            break;
        default: 
            ret = nrrdTypeUnknown;
    }

    return ret;
}

// set values as 3x3 ItoW matrix
static void setItoW3D(double *ItoW, const Nrrd *nin) 
{
    /* 0  1  2  3
       4  5  6  7
       8  9  10 11
       12 13 14 15 */

    // first column
    ItoW[0] = (double)(nin->axis[1].spaceDirection[0]);
    ItoW[4] = (double)(nin->axis[1].spaceDirection[1]);
    ItoW[8] = (double)(nin->axis[1].spaceDirection[2]);
    ItoW[12] = 0;

    // second column
    ItoW[1] = (double)(nin->axis[2].spaceDirection[0]);
    ItoW[5] = (double)(nin->axis[2].spaceDirection[1]);
    ItoW[9] = (double)(nin->axis[2].spaceDirection[2]);
    ItoW[13] = 0;

    // third column
    ItoW[2] = (double)(nin->axis[3].spaceDirection[0]);
    ItoW[6] = (double)(nin->axis[3].spaceDirection[1]);
    ItoW[10] = (double)(nin->axis[3].spaceDirection[2]);
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

        return 1;
    }

    // check if the last row is 0 0 1
    if (!( 0 == ItoW[12] &&
           0 == ItoW[13] &&
           0 == ItoW[14] &&
           1 == ItoW[15] )) 
    {
        printf("%s: ItoW matrix last row [%g,%g,%g, %g] not [0,0,0,1]\n", __func__, ItoW[12], ItoW[13], ItoW[14], ItoW[15]);
        return 1;
    }

    // else no problems
    return 0;
}

// get the size of a type
static size_t lsptypeSize(lspType dtype) 
{
    size_t ret;
    switch (dtype) 
    {
        case lspTypeUChar: ret = sizeof(unsigned char);
            break;
        case lspTypeUShort: ret = sizeof(unsigned short);
            break;
        case lspTypeShort: ret = sizeof(short);
            break;
        case lspTypeDouble: ret = sizeof(double);
            break;
        // unknown case
        default: ret = 0;
    }
    return ret;
}

// check if the loaded Nrrd data is legit
static int lspNrrdDataCheck(const Nrrd *nin) 
{
    if (nrrdCheck(nin)) 
    {
        printf("%s: problem with nrrd itself\n", __func__);
        return 1;
    }
    // the type should at least fall into one
    if (!( nrrdTypeUChar == nin->type 
          || nrrdTypeShort == nin->type 
          || nrrdTypeUShort == nin->type 
          || nrrdTypeDouble == nin->type )) 
    {
        printf("%s: can't handle nrrd type %s (need %s, %s, %s or %s)\n", __func__, airEnumStr(nrrdType, nin->type),
                 airEnumStr(nrrdType, nrrdTypeUChar),
                 airEnumStr(nrrdType, nrrdTypeShort),
                 airEnumStr(nrrdType, nrrdTypeUShort),
                 airEnumStr(nrrdType, nrrdTypeDouble));
        
        return 1;
    }

    if (!( nin->dim == 4 )) 
    {
        printf("%s: got dimension %u, not 4\n", __func__, nin->dim);
        return 1;
    }

    // It should be 2-channel data (GFP and RFP)
    if ( nin->axis[0].size != 2 )
    {
        printf("%s: the data loaded is now two-channel data, instead its channel is %u\n", __func__, (uint)(nin->axis[0].size));
        return 1;
    }
    
    // check the space of the loaded nrrd data
    if (airEnumValCheck(nrrdSpace, nin->space)) 
    {
        printf("%s: array space %u not set or known\n", __func__, nin->space);
        return 1;
    }

    // from skim we set it to 3D-right-handed
    // if (nin->space != nrrdSpaceRightUp) 
    // {
    //     printf("%s: array space %s not expected as %s\n", __func__,
    //              airEnumStr(nrrdSpace, nin->space),
    //              airEnumStr(nrrdSpace, nrrdSpaceRightUp));
    //     return 1;
    // }

    // set the homogeneous transformation from volume index-space to world-space
    // check if the loaded Nrrd data can give us the correct Itow
    double ItoW[16];
    setItoW3D(ItoW, nin);
    if (ItoW3DCheck(ItoW)) 
    {
        printf("%s: problem with ItoW\n", __func__);
        return 1;
    }
    
    // make sure that all the sampling are center sampling
    // axis 1 2 3 are the three spatial axis (axis 0 is the channel)
    if (!( nin->axis[1].center == nrrdCenterCell
           && nin->axis[2].center == nrrdCenterCell
           && nin->axis[3].center == nrrdCenterCell ))
    {
        printf("%s: axis[%u, %u, %u] centering not all %s", __func__,
                 1, 2, 3, airEnumStr(nrrdCenter, nrrdCenterCell));
        return 1;
    }

    // no problem
    return 0;
}

// check the data content
static int metaDataCheck(uint channel, uint size0, uint size1, uint size2, const double *ItoW, lspType dtype) 
{
    if ( channel != 2 ) 
    {
        printf("%s: invalid channel value %u, Nrrd data should have 2 channels\n", __func__, channel);
        return 1;
    }
    if ( ( size0<=0 && size1<=0 && size2<=0) ) 
    {
        printf("%s: invalid volume sizes (%u, %u, %u)\n", __func__, size0, size1, size2);
        return 1;
    }
    if ( airEnumValCheck(lspType_ae, dtype) ) 
    {
        printf("%s: invalid type %d\n", __func__, dtype);
        return 1;
    }
    if (ItoW && ItoW3DCheck(ItoW)) 
    { // only call ItoWCheck() on non-NULL ItoW
        printf("%s: problem with ItoW matrix\n", __func__);
        return 1;
    }

    // looks good
    return 0;
}

// helper function that loads the grid txt file and generate 4x4 NewItoW transformation
static int processGrid (double* NewItoW, uint* boundaries, const std::string gridPath)
{
    // get the input stream reading the file
    ifstream gridStream(gridPath);
    string curLine;
    // all the numbers we read from txt file
    vector<double> allNumbers;

    if (gridStream.is_open())
    {
        while (getline(gridStream, curLine))
        {
            // make the current string line a stream
            istringstream curStream(curLine);

            // parse the line into doubles
            for (string s; curStream >> s;)
            {
                allNumbers.push_back(stod(s));
            }
        }
    }
    else
    {
        cout << "Unable to open file" << endl;
        return 1;
    }

    // we should have 16 numbers
    if (allNumbers.size() != 16)
    {
        cout << "Did not read 16 numbers from grid txt file, something was wrong" << endl;
        return 1;
    }

    // print the loaded grid.txt
    // cout << "Loaded grid txt file at " << gridPath << " is:" << endl;
    // for (size_t i = 0; i < allNumbers.size(); i++)
    // {
    //     cout << allNumbers[i] << " ";
    //     if ((i+1)%4 == 0)
    //     {
    //         cout << endl;
    //     }
    // }

    // construct NewItoW
    /*
    general form from grid file (index in paranthesis)
      3(0)     ox(1)     oy(2)    oz(3)
    sz0(4)    e0x(5)    e0y(6)   e0z(7)
    sz1(8)    e1x(9)    e1y(10)  e1z(11)
    sz2(12)   e2x(13)   e2y(14)  e2z(15)

    the index-to-world (index in the resampling grid index-space) transform：
    e0x  e1x  e2x  ox
    e0y  e1y  e2y  oy
    e0z  e1z  e2z  oz
    0    0   0    1
    */
    NewItoW[0] = allNumbers[5];
    NewItoW[1] = allNumbers[9];
    NewItoW[2] = allNumbers[13];
    NewItoW[3] = allNumbers[1];

    NewItoW[4] = allNumbers[6];
    NewItoW[5] = allNumbers[10];
    NewItoW[6] = allNumbers[14];
    NewItoW[7] = allNumbers[2];

    NewItoW[8] = allNumbers[7];
    NewItoW[9] = allNumbers[11];
    NewItoW[10] = allNumbers[15];
    NewItoW[11] = allNumbers[3];

    NewItoW[12] = 0.;
    NewItoW[13] = 0.;
    NewItoW[14] = 0.;
    NewItoW[15] = 0.;

    // get the boundary information
    boundaries[0] = (uint)allNumbers[4];
    boundaries[1] = (uint)allNumbers[8];
    boundaries[2] = (uint)allNumbers[12];


    return 0;
    
}

static void m4_affine_inv(double inv[16], const double mat[16]) 
{
    // get the 3x3 matrix R to be the upper diagonal 3x3 sub-matrix of 4x4 matrix mat
    double R[9];
    M34_UPPER(R, mat);
    // get 3-vector t from mat
    double t[3];
    V3_SET(t, mat[3], mat[7], mat[11]);

    // take the inverse of R
    double R_inv[9];
    double TMP;
    M3_INVERSE(R_inv, R, TMP);
    // compute the 3-vector R_inv * t
    double R_invT[3];
    MV3_MUL(R_invT, R_inv, t);

    // now we assemble the output
    M4_SET(inv, R_inv[0], R_inv[1], R_inv[2], -R_invT[0],
                R_inv[3], R_inv[4], R_inv[5], -R_invT[1],
                R_inv[6], R_inv[7], R_inv[8], -R_invT[2],
                0, 0, 0, 1);

    // ^'^'^'^'^'^'^'^'^'^'^'^'^'^'^  end student code (10L in ref)
    return;
}

// *************************** End of static functions ******************

// initialize a new volume
lspVolume* lspVolumeNew() 
{
    lspVolume *ret = MALLOC(1, lspVolume);
    assert(ret);
    ret->content = NULL;
    ret->channel = ret->size[0] = ret->size[1] = ret->size[2] = 0;
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
int lspVolumeAlloc(lspVolume *vol, uint channel, uint size0, uint size1, uint size2, lspType dtype) 
{
    // some error handlings
    if (!vol) 
    {
        printf("%s: got NULL pointer\n", __func__);
        return 1;
    }

    if (metaDataCheck(channel, size0, size1, size2, NULL, dtype)) 
    {
        printf("%s: problem with meta-data\n", __func__);
        return 1;
    }

    // if true, means that we need to allocate new memory, no re-use applicable
    int doalloc;
    if (!(vol->data.vd)) 
    {
        doalloc = 1;
    }
    // already allocated, but not the right size/type
    else if (vol->channel != channel
               || vol->size[0] != size0
               || vol->size[1] != size1
               || vol->size[2] != size2
               || vol->dtype != dtype) 
    {
        // we know that vol->data.vd is not NULL, safely free it
        free(vol->data.vd);
        doalloc = 1;
    } 
    // re-use existing allocating
    else
    {
        doalloc = 0;
    }

    // allocate new
    if (doalloc) 
    {
        vol->data.vd = calloc(channel*size0*size1*size2, lsptypeSize(dtype));

        // a little check
        if (!vol->data.vd) 
        {
            printf("%s: failed to allocate %u * %u * %u * %u %s", __func__,
                     channel, size0, size1, size2, airEnumStr(lspType_ae, dtype));
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
        printf("%s: got NULL pointer (%p, %p)\n", __func__, (void*)vol, (void*)nin);
        return 1;
    }

    // check loaded Nrrd data, see if it would fit into a volume
    if (lspNrrdDataCheck(nin)) 
    {
        printf("%s: given array doesn't conform to a lsp volume\n", __func__);
        return 1;
    }
    
    uint dim;

    // change Nrrd type to lsp type
    lspType ltype = typeNRRDtoLSP(nin->type);
    
    // allocate memory for volume
    if (lspVolumeAlloc(vol, nin->axis[0].size, nin->axis[1].size, nin->axis[2].size, nin->axis[3].size, ltype)) 
    {
        printf("%s: trouble allocating volume\n", __func__);
        return 1;
    }

    // convert content (description)
    if (nin->content) 
    {
        vol->content = airStrdup(nin->content);
    }
    else
    {
        printf("%s: Input Nrrd data does not have content\n", __func__);
        return 1;
    }

    // convert actual data
    uint elSize = (uint)nrrdElementSize(nin);
    uint elNum = (uint)nrrdElementNumber(nin);

    if (ltype == lspTypeUChar) 
    {
        memcpy(vol->data.vd, nin->data, elSize*elNum);
    }
    // we want to have unsigned short become short since negativity would be useful later
    else if (ltype == lspTypeUShort || ltype == lspTypeShort)
    {
        memcpy(vol->data.s, nin->data, elSize*elNum);
    }
    else if (ltype == lspTypeDouble)
    {
        memcpy(vol->data.dl, nin->data, elSize*elNum);
    }
    else
    {
        cout << "lspVolumeFromNrrd: Unknown data type" << endl;
        return 1;
    }

    // set the ItoW matrix
    setItoW3D(vol->ItoW, nin);

    return 0;
}

// converts volume into Nrrd
int lspNrrdFromVolume(Nrrd *nout, const lspVolume *vol) 
{
    if (!(nout && vol)) 
    {
        printf("%s: got NULL pointer (%p, %p)\n", __func__, (void*)nout, (void*)vol);
        return 1;
    }

    if ( vol->channel != 2) 
    {
        printf("%s: volume should have 2 channels, instead got %u\n", __func__, vol->channel);
        return 1;
    }

    // channel + 3 spatial axis is 4 dimemsion
    uint ndim = 4;
    size_t size[4] = {vol->channel, vol->size[0], vol->size[1], vol->size[2]};
    
    if ( nrrdWrap_nva(nout, vol->data.vd, typeLSPtoNRRD(vol->dtype), ndim, size) ) 
    {
        printf("%s: trouble wrapping data in nrrd", __func__);
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
    nout->axis[1].spaceDirection[0] = vol->ItoW[0];
    nout->axis[1].spaceDirection[1] = vol->ItoW[4];
    nout->axis[1].spaceDirection[2] = vol->ItoW[8];
    // y-direction
    nout->axis[2].spaceDirection[0] = vol->ItoW[1];
    nout->axis[2].spaceDirection[1] = vol->ItoW[5];
    nout->axis[2].spaceDirection[2] = vol->ItoW[9];
    // z-direction
    nout->axis[2].spaceDirection[0] = vol->ItoW[2];
    nout->axis[2].spaceDirection[1] = vol->ItoW[6];
    nout->axis[2].spaceDirection[2] = vol->ItoW[10];

    // origin
    nout->spaceOrigin[0] = vol->ItoW[3];
    nout->spaceOrigin[1] = vol->ItoW[7];
    nout->spaceOrigin[2] = vol->ItoW[11];
    
    // sampling method
    nout->axis[1].center = nrrdCenterCell;
    nout->axis[2].center = nrrdCenterCell;
    nout->axis[3].center = nrrdCenterCell;

    return 0;
}

/*
  lspCtx3DNew creates the context to contain all resources and state
  associated with 3D convolution, which is computing on a given volume "vol", 
  and a reconstruction kernel "kernel"
*/
lspCtx3D* lspCtx3DNew(const lspVolume* vol, const std::string gridPath, const NrrdKernel* kernel, const double* vmm, airArray* mop) 
{
    // some error checks
    if (!(vol && kernel)) 
    {
        printf("%s: got NULL pointer (%p,%p)", __func__, (void*)vol, (void*)kernel);
        return NULL;
    }
    if ( vol->channel != 2) 
    {
        printf("%s: only works on 2-channel (not %u-channel) volumes", __func__, vol->channel);
        return NULL;
    }
    
    // allocate memory
    lspCtx3D *ctx3D = MALLOC(1, lspCtx3D);
    assert(ctx3D);
    ctx3D->verbose = 0;
    ctx3D->volume = vol;
    ctx3D->kern = kernel;
    ctx3D->inside = 1;
    ctx3D->volMinMax[0] = vmm ? vmm[0] : lspNan(0);
    ctx3D->volMinMax[1] = vmm ? vmm[1] : lspNan(0);
    ctx3D->volMinMax[2] = vmm ? vmm[2] : lspNan(0);

    // parse the kernel
    ctx3D->kernelSpec = nrrdKernelSpecNew();
    ctx3D->mop_v = airMopNew();
    airMopAdd(ctx3D->mop_v, ctx3D->kernelSpec, (airMopper)nrrdKernelSpecNix, airMopAlways);
    if (nrrdKernelParse(&(ctx3D->kernelSpec->kernel), ctx3D->kernelSpec->parm, ctx3D->kern->name))
    {
        printf("%s: trouble parsing kernel\n", __func__);
        airMopError(ctx3D->mop_v);
    }

    // copy vol->ItoW for easier access and consistancy
    M4_COPY(ctx3D->ItoW, ctx3D->volume->ItoW);

    // convert wpos to index-space
    // sets 4x4 WtoI to inverse of 4x4 ctx->ItoW
    m4_affine_inv(ctx3D->WtoI, ctx3D->ItoW);

    // read the grid.txt and fill in NewItoW, which is the transformation from cropped index space to world
    processGrid(ctx3D->NewItoW, ctx3D->boundaries, gridPath);

    return ctx3D;
}

// free a lspCtx3D
lspCtx3D* lspCtx3DNix(lspCtx3D* ctx)
{

    if (ctx) 
    {
        // free(ctx->kernelSpec);
        airMopOkay(ctx->mop_v);
        free(ctx);
    }
    return NULL;
}