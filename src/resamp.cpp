// The program performs resampling process on images genereated by anim.cpp
// Created by Zhuokai Zhao
// Contact: zhuokai@uchicago.edu


#include <teem/nrrd.h>
#include <opencv2/opencv.hpp>
#include <omp.h>
#include <fstream>
#include <regex>
#include <algorithm>
#include <limits>

#include "util.h"
#include "skimczi.h"
#include "resamp.h"
#include "lsp_math.h"
#include "image.h"
#include "volume.h"

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

#include <chrono>

using namespace std;
namespace fs = boost::filesystem;

// box kernel
#define _KDEF(NAME, DESC, SUPP)                                       \
    static lspKernel NAME##Kernel = { #NAME, DESC, SUPP, NAME##Eval,  \
                                        NAME##Apply }
#define KDEF(NAME, DESC, SUPP)                                   \
    _KDEF(NAME, DESC, SUPP);                                     \
    const lspKernel *const lspKernel##NAME = &(NAME##Kernel)

static double BoxEval(double xx) 
{
    return (xx < -0.5
            ? 0
            : (xx < 0.5
               ? 1
               : 0));
}
KDEF(Box, "For nearest-neighbor interpolation", 1);

// CTMR kernel
static double CtmrEval(double x) 
{
    double ret;
    x = fabs(x);
    if (x < 1) 
    {
        ret = 1 + x*x*(-5.0/2 + x*(3.0/2));
    } 
    else if (x < 2) 
    {
        x -= 1;
        ret = x*(-1.0/2 + x*(1 - x/2));
    } 
    else 
    {
        ret = 0;
    }
    return ret;
}
KDEF(Ctmr, "Catmull-Rom spline (C1, reconstructs quadratic)", 4);

void setup_resamp(CLI::App &app)
{
    auto opt = std::make_shared<resampOptions>();
    auto sub = app.add_subcommand("resamp", "Perform resampling on input images");

    //sub->add_option("-i, --image_path", opt->image_path, "Path that includes input images")->required();
    // sub->add_option("-t, --image_type", sopt->image_type, "Type of the image, max or ave")->required();
    sub->add_option("-i, --nhdr_path", opt->nhdr_path, "Path of input nrrd header files.")->required();
    
    sub->add_option("-g, --kernel", opt->grid_path, "Path that includes the grid file")->required();
    sub->add_option("-o, --out_path", opt->out_path, "Path that includes all the output images")->required();
    sub->add_option("-n, --max_file_number", opt->maxFileNum, "The max number of files that we want to process");

    sub->set_callback([opt]() 
    { 
        // first determine if input nhdr_path is valid
        if (checkIfDirectory(opt->nhdr_path))
        {
            cout << "nhdr input directory " << opt->nhdr_path << " is valid" << endl;
            
            // count the number of files
            const vector<string> files = GetDirectoryFiles(opt->nhdr_path);
            
            // note that the number starts counting at 0
            int nhdrNum = 0;
            
            for (int i = 0; i < files.size(); i++) 
            {
                string curFile = files[i];
                // check if input file is a .nhdr file
                int nhdr_suff = curFile.rfind(".nhdr");
                if ( (nhdr_suff != string::npos) && (nhdr_suff == curFile.length() - 5))
                {
                    if (opt->verbose)
                        cout << "Current input file " + curFile + " ends with .nhdr, count this file" << endl;
                    
                    nhdrNum++;
                
                    // now we need to understand the sequence number of this file
                    int start = -1;
                    int end = curFile.rfind(".nhdr");
                    // current file name without type
                    string curFileName = curFile.substr(0, end);
                    
                    // The sequenceNumString will have zero padding, like 001
                    for (int i = 0; i < end; i++)
                    {
                        // we get the first position that zero padding ends
                        if (curFile[i] != '0')
                        {
                            start = i;
                            break;
                        }
                    }
        
                    string sequenceNumString;
                    // for the case that it is just 000 which represents the initial time stamp
                    if (start == -1)
                    {
                        sequenceNumString = "0";
                    }
                    else
                    {
                        int length = end - start;
                        sequenceNumString = curFile.substr(start, length);
                    }

                    if (is_number(sequenceNumString))
                    {
                        opt->allValidFiles.push_back( make_pair(stoi(sequenceNumString), curFileName) );
                    }
                    else
                    {
                        cout << "WARNING: " << sequenceNumString << " is NOT a number" << endl;
                    }
                }

            }

            // after finding all the files, sort the allValidFiles in ascending order
            sort(opt->allValidFiles.begin(), opt->allValidFiles.end());

            cout << nhdrNum << " .nhdr files found in input path " << opt->nhdr_path << endl << endl;

            // sanity check
            if (nhdrNum != opt->allValidFiles.size())
            {
                cout << "ERROR: Not all valid files have been recorded" << endl;
            }

            // if the user restricts the number of files to process
            if (!opt->maxFileNum.empty())
            {
                nhdrNum = stoi(opt->maxFileNum);
            }
                
            // update file number
            opt->file_number = nhdrNum;
            cout << "Total number of .nhdr files that we are processing is: " << opt->tmax << endl << endl;

            try
            {
                auto start = chrono::high_resolution_clock::now();
                Resamp(*opt).main();
                auto stop = chrono::high_resolution_clock::now(); 
                auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 
                cout << endl << "Processing took " << duration.count() << " seconds" << endl << endl; 
            }
            catch(LSPException &e)
            {
                std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
            }
        }
        else
        {
            // the program also handles if input file is a single file
            cout << opt->nhdr_path << " is not a directory, check if it is a valid .nhdr file" << endl;
            const string curFile = opt->nhdr_path;

            std::cout << "Current file name is: " << curFile << endl;
            
            // check if input file is a .nhdr file
            int suff = curFile.rfind(".nhdr");

            if ( (suff == string::npos) || (suff != curFile.length() - 5)) 
            {
                cout << "Current input file " + curFile + " does not end with .nhdr, error" << endl;
                return;
            }
            else
            {
                cout << "Current input file " + curFile + " ends with .nhdr, process this file" << endl;

                // update file number
                opt->file_number = -1;    

                try
                {
                    auto start = chrono::high_resolution_clock::now();
                    Resamp(*opt).main();
                    auto stop = chrono::high_resolution_clock::now(); 
                    auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 
                    cout << endl << "Processing took " << duration.count() << " seconds" << endl << endl; 
                }
                catch(LSPException &e)
                {
                    std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
                }
            }
        }
        
    });
}


Resamp::Resamp(resampOptions const &opt): opt(opt), mop(airMopNew())
{
    // create folder if it does not exist
    // if (!checkIfDirectory(opt.out_path))
    // {
    //     boost::filesystem::create_directory(opt.out_path);
    //     cout << "Image output path " << opt.out_path << " does not exits, but has been created" << endl;
    // }
}

Resamp::~Resamp()
{
    airMopOkay(mop);
}

// ********************** some static helper functions **********************
static int isEven (uint x)
{
    if (x % 2 == 0)
    {
        return 1;
    }

    return 0;
}

// ********************** end of static helper functions *********************

void Resamp::ConvoEval2D(lspCtx2D *ctx2D, double xw, double yw) 
{
    // initialize output
    ctx2D->wpos[0] = xw;
    ctx2D->wpos[1] = yw;
    ctx2D->outside = 0;
    ctx2D->value = lspNan(0);

    // first convert wpos to ipos, where ipos are x1 and x2 as in FSV
    // MV3_MUL only takes 3-vector
    double ipos[3];
    double wpos[3] = {ctx2D->wpos[0], ctx2D->wpos[1], 1};

    // 3-vector ipos = 3x3 matrix WtoI * 3-vector wpos
    MV3_MUL(ipos, ctx2D->WtoI, wpos);

    // set this to ctx
    ctx2D->ipos[0] = ipos[0];
    ctx2D->ipos[1] = ipos[1];

    // determine different n1, n2 for even and odd kernels
    int n1, n2;
    // determine lower and upper bounds for later convolution
    int lower, upper;

    // even kernel
    if ( isEven(ctx2D->kern->support) )
    {
        // n1 = floor(x1), n2 = floor(x2)
        n1 = floor(ctx2D->ipos[0]);
        n2 = floor(ctx2D->ipos[1]);
        // lower = 1 - support/2
        lower = 1 - (int)ctx2D->kern->support / 2;
        // upper = support/2
        upper = (int)ctx2D->kern->support / 2;
    }
    // odd kernel
    else
    {
        // n1 = floor(x1+0.5), n2 = floor(x2+0.5)
        n1 = floor(ctx2D->ipos[0] + 0.5);
        n2 = floor(ctx2D->ipos[1] + 0.5);
        // lower = (1 - support)/2
        lower = (int)(1 - ctx2D->kern->support) / 2;
        // upper = (support - 1)/2
        upper = (int)(ctx2D->kern->support - 1) / 2;
    }

    // calculate alpha based on n1, n2
    double alpha1, alpha2;
    alpha1 = ctx2D->ipos[0] - n1;
    alpha2 = ctx2D->ipos[1] - n2;

    // separable convolution
    double sum = 0;
    // double sum_d1 = 0, sum_d2 = 0;

    // initialize kernels
    double k1[ctx2D->kern->support], k2[ctx2D->kern->support];
    // kernel for derivatives (kern->deriv points back to itself when no gradient)
    // real k1_d[ctx->kern->deriv->support], k2_d[ctx->kern->deriv->support];

    // precompute two vectors of kernel evaluations to save time
    for (int i = lower; i <= upper; i++)
    {
        k1[i - lower] = ctx2D->kern->eval(alpha1 - i);
        k2[i - lower] = ctx2D->kern->eval(alpha2 - i);

        // if gradient is true, kernel is calculated with gradient
        // if (needgrad)
        // {
        //     k1_d[i - lower] = ctx->kern->deriv->eval(alpha1 - i);
        //     k2_d[i - lower] = ctx->kern->deriv->eval(alpha2 - i);
        // }
    }

    // compute via two nested loops over the 2D-kernel support
    // make sure image is not empty
    if (ctx2D->image != NULL)
    {
        // check for potential outside
        // Notice that both direction should be checked separately
        for (int i1 = lower; i1 <= upper; i1++)
        {
            if ( i1 + n1 < 0 || i1 + n1 >= (int)ctx2D->image->size[0] )
            {
                ctx2D->outside += 1;
            }
        }
        for (int i2 = lower; i2 <= upper; i2++)
        {
            if ( i2 + n2 < 0 || i2 + n2 >= (int)ctx2D->image->size[1] )
            {
                ctx2D->outside += 1;
            }
        }

        if (ctx2D->outside == 0)
        {
            // faster axis first (i1 fast)
            for (int i2 = lower; i2 <= upper; i2++)
            {
                for (int i1 = lower; i1 <= upper; i1++)
                {
                    // not outside
                    sum = sum + ctx2D->image->data.dl[ (n2+i2)*ctx2D->image->size[0] + n1 + i1 ] * k1[i1-lower] * k2[i2-lower];

                    // if (needgrad)
                    // {
                    //     sum_d1 = sum_d1 + ctx->image->data.rl[ (n2+i2)*ctx->image->size[0] + n1 + i1 ] * k1_d[i1-lower] * k2[i2-lower];
                    //     sum_d2 = sum_d2 + ctx->image->data.rl[ (n2+i2)*ctx->image->size[0] + n1 + i1 ] * k1[i1-lower] * k2_d[i2-lower];
                    // }
                }
            }

            // if no outside, assign value
            ctx2D->value = sum;
            // derivative
            // if (needgrad)
            // {
            //     // convert from index-space to world space
            //     real gradient_w[2];
            //     real gradient_i[2] = {sum_d1, sum_d2};
            //     MV2_MUL(gradient_w, ctx->ItoW_d, gradient_i);

            //     // save the *world-space* gradient result in ctx->gradient
            //     V2_COPY(ctx->gradient, gradient_w);
            // }
        }
    }

    return;
}

// function that performs 2D resampling
Nrrd* nrrdResample2D(Nrrd* nin, uint axis, NrrdKernel* kernel, double* kparm)
{
    // initialize the output result
    Nrrd* nout = safe_nrrd_new(mop, (airMopper)nrrdNuke);

    // set input to resample context
    NrrdResampleContext* resampContext = nrrdResampleContextNew();
    airMopAdd(mop, resampContext, (airMopper)nrrdResampleContextNix, airMopAlways);
    nrrdResampleInputSet(resampContext, nin);
    
    // get the sample ready
    nrrdResampleSamplesSet(resampContext, axis, nin->axis[axis].size);
    nrrdResampleRangeFullSet(resampContext, axis);
    // nrrdBoundaryBleed: copy the last/first value out as needed
    nrrdResampleBoundarySet(resampContext, nrrdBoundaryBleed);
    
    // set the kernel
    nrrdResampleKernelSet(resampContext, axis, kernel, kparm);

    // renormalizing
    nrrdResampleRenormalizeSet(resampContext, AIR_TRUE)

    // start the resampling
    nrrdResampleExecute(resampContext, nout);

    return nout;

}

void Resamp::ConvoEval3D(lspCtx3D *ctx3D, double xw, double yw, double zw)
{
    // initialize output
    ctx3D->wpos[0] = xw;
    ctx3D->wpos[1] = yw;
    ctx3D->wpos[2] = zw;
    ctx3D->outside = 0;
    ctx3D->value = lspNan(0);

    // first convert wpos to ipos, where ipos are x1, x2 and x3 as in FSV
    // MV4_MUL only takes 4-vector
    double ipos[4];
    double wpos[4] = {ctx3D->wpos[0], ctx3D->wpos[1], ctx3D->wpos[2], 1};

    // 4-vector ipos = 4x4 matrix WtoI * 4-vector wpos
    MV4_MUL(ipos, ctx3D->WtoI, wpos);

    // set this to ctx
    ctx3D->ipos[0] = ipos[0];
    ctx3D->ipos[1] = ipos[1];
    ctx3D->ipos[2] = ipos[2];

    // determine different n1, n2 and n3 for even and odd kernels
    int n1, n2, n3;
    // determine lower and upper bounds for later convolution
    int lower, upper;

    // even kernel
    if ( isEven(ctx3D->kern->support) )
    {
        // n1 = floor(x1), n2 = floor(x2), n3 = floor(n3)
        n1 = floor(ctx3D->ipos[0]);
        n2 = floor(ctx3D->ipos[1]);
        n3 = floor(ctx3D->ipos[2]);
        // lower = 1 - support/2
        lower = 1 - (int)ctx3D->kern->support / 2;
        // upper = support/2
        upper = (int)ctx3D->kern->support / 2;
    }
    // odd kernel
    else
    {
        // n1 = floor(x1+0.5), n2 = floor(x2+0.5), n3 = floor(x3+0.5)
        n1 = floor(ctx3D->ipos[0] + 0.5);
        n2 = floor(ctx3D->ipos[1] + 0.5);
        n3 = floor(ctx3D->ipos[2] + 0.5);
        // lower = (1 - support)/2
        lower = (int)(1 - ctx3D->kern->support) / 2;
        // upper = (support - 1)/2
        upper = (int)(ctx3D->kern->support - 1) / 2;
    }

    // calculate alpha based on n1, n2 and n3
    double alpha1, alpha2, alpha3;
    alpha1 = ctx3D->ipos[0] - n1;
    alpha2 = ctx3D->ipos[1] - n2;
    alpha3 = ctx3D->ipos[2] - n3;

    // separable convolution
    double sum = 0;
    // double sum_d1 = 0, sum_d2 = 0;

    // initialize kernels
    double k1[ctx3D->kern->support], k2[ctx3D->kern->support], k3[ctx3D->kern->support];
    // kernel for derivatives (kern->deriv points back to itself when no gradient)
    // real k1_d[ctx->kern->deriv->support], k2_d[ctx->kern->deriv->support];

    // precompute two vectors of kernel evaluations to save time
    for (int i = lower; i <= upper; i++)
    {
        k1[i - lower] = ctx3D->kern->eval(alpha1 - i);
        k2[i - lower] = ctx3D->kern->eval(alpha2 - i);
        k3[i - lower] = ctx3D->kern->eval(alpha3 - i);
    }

    // compute via three nested loops over the 3D-kernel support
    // make sure volume is not empty
    if (ctx3D->volume != NULL)
    {
        // check for potential outside
        // Notice that both direction should be checked separately
        for (int i1 = lower; i1 <= upper; i1++)
        {
            if ( i1 + n1 < 0 || i1 + n1 >= (int)ctx3D->volume->size[0] )
            {
                ctx3D->outside += 1;
            }
        }
        for (int i2 = lower; i2 <= upper; i2++)
        {
            if ( i2 + n2 < 0 || i2 + n2 >= (int)ctx3D->volume->size[1] )
            {
                ctx3D->outside += 1;
            }
        }
        for (int i3 = lower; i3 <= upper; i3++)
        {
            if ( i3 + n3 < 0 || i3 + n3 >= (int)ctx3D->volume->size[2] )
            {
                ctx3D->outside += 1;
            }
        }

        // no outside found
        if (ctx3D->outside == 0)
        {
            // faster axis first (i1 fast)
            for (int i3 = lower; i3 <= upper; i3++)
            {
                for (int i2 = lower; i2 <= upper; i2++)
                {
                    for (int i1 = lower; i1 <= upper; i1++)
                    {
                        // compute data index
                        uint data_index = (n3+i3)*(ctx3D->volume->size[1]*ctx3D->volume->size[0]) + (n2+i2)*(ctx3D->volume->size[0]) + n1 + i1;
                        // not outside
                        sum = sum + ctx3D->volume->data.dl[data_index] * k1[i1-lower] * k2[i2-lower] * k3[i3-lower];
                    }
                }
            }

            // if no outside, assign value
            ctx3D->value = sum;
        }
    }

    return; 

}

// function that performs 3D resampling (convolution)
void nrrdResample3D(lspVolume* newVolume, lspCtx3D* ctx3D)
{
    // input volume
    lspVolume* volume = ctx3D->volume;
    // sizes in x, y and z directions
    uint sizeX = volume.size[0];
    uint sizeY = volume.size[1];
    uint sizeZ = volume.size[2];

    // evaluate at each world-space position
    for (int z = 0; z < sizeZ; z++)
    {
        for (int y = 0; y < sizeY; y++)
        {
            for (int x = 0; x < sizeX; x++)
            {
                newVolume[z*sizeZ + y*sizeY + x] = ConvoEval3D(ctx3D, x, y, z);
            }
        }
    }

}

// main function
void Resamp::main()
{
    // single file mode
    if (opt.file_number == -1)
    {
        // nhdr_path is just the nhdr file namae instead of a folder path
        string nhdr_name = opt.nhdr_path;
        // load the nhdr header
        Nrrd* nin = safe_nrrd_load(mop, nhdr_name);

        // permute from x(0)-y(1)-channel(2)-z(3) to channel(2)-x(0)-y(1)-z(3)
        unsigned int permute[4] = {2, 0, 1, 3};

        // do the permutation
        Nrrd* nin_permuted = safe_nrrd_new(mop, (airMopper)nrrdNuke);
        nrrdAxesPermute(nin_permuted, nin, permute);

        // put Nrrd data into lspVolume
        lspVolume* volume = lspVolumeNew();
        airMopAdd(mop, volume, (airMopper)lspVolumeNix, airMopAlways);
        lspVolumeFromNrrd(vol, nin_permuted);

        // get box kernel
        lspKernel* box = lspKernelBox;

        // put both volume and box kernel into the Ctx3D
        lspCtx3D* ctxBox = lspCtx3DNew(volume, box, NULL /* imm */);

        // perform the 3D sampling (convolution)
        // resulting volume_box
        lspVolume* volume_box = lspVolumeNew();
        airMopAdd(mop, volume_box, (airMopper)lspVolumeNix, airMopAlways);
        nrrdResample3D(volume_box, ctxBox);

        // Catmull-Rom kernel, which has 4 sample support and is 2-accurate
        lspKernel* ctmr = lspKernelCtmr;
        
        // put both volume_box and ctmr kernel into the Ctx3D
        lspCtx3D* ctxCtmr = lspCtx3DNew(volume_box, ctmr, NULL /* imm */);

        // perform the 3D sampling (convolution)
        // resulting volume_ctmr
        lspVolume* volume_ctmr = lspVolumeNew();
        airMopAdd(mop, volume_ctmr, (airMopper)lspVolumeNix, airMopAlways);
        nrrdResample3D(volume_ctmr, ctxCtmr);

        // change the volume back to Nrrd file for projection
        Nrrd* nout = safe_nrrd_new(mop, (airMopper)nrrdNuke);
        lspVolumeNrrdWrap(nout, volume_ctmr);

        // Project the volume alone z axis using MIP
        Nrrd* projNrrd = safe_nrrd_new(mop, (airMopper)nrrdNuke);
        nrrdProject(projNrrd, nout, 3, nrrdMeasureMax, nrrdTypeDouble);
        
        // slice the nrrd into separate GFP and RFP channel (and quantize to 8bit)
        Nrrd* slices = {safe_nrrd_new(mop, (airMopper)nrrdNuke),
                        safe_nrrd_new(mop, (airMopper)nrrdNuke)};
        // quantized
        Nrrd* quantized = {safe_nrrd_new(mop_t, (airMopper)nrrdNuke),
                           safe_nrrd_new(mop_t, (airMopper)nrrdNuke)};

        // range during quantizing
        auto range = nrrdRangeNew(lspNan(0), lspNan(0));
        airMopAdd(mop, range, (airMopper)nrrdRangeNix, airMopAlways);

        for (int i = 0; i < 2; i++)
        {
            nrrdSlice(slices[i], projNrrd, 0, i);
            nrrdRangePercentileFromStringSet(range0, slices[i],  "0.1%", "0.1%", 5000, true);
            nrrdQuantize(quantized[i], slices[i], range, 8);
        }

        // Join the two channel
        Nrrd* finalJoined = safe_nrrd_new(mop, (airMopper)nrrdNuke);
        nrrdJoin(finalJoined, quantized, 2, 0, 1)

        // save the final nrrd as image
        if (nrrdSave(opt.out_path.c_str(), finalJoined, NULL)) 
        {
            printf("%s: trouble saving output\n", __func__);
            airMopError(mop);
            return;
        }

    }
    
    
    
    
    // // the number of images
    // int imageNum = opt.imageNum;

    // // if imageNum == -1, single image mode
    // if (imageNum == -1)
    // {
    //     // load the image first
    //     lspImage* image = lspImageNew();
    //     airMopAdd(mop, image, (airMopper)lspImageNix, airMopAlways);
        
    //     int loadImageFail = lspImageLoad(image, opt.image_path.c_str());
        
    //     if (loadImageFail)
    //     {
    //         cout << "Error loading image with path " << opt.image_path << endl;
    //         return;
    //     }
    //     else
    //     {
    //         cout << "Image " << opt.image_path << " has been loaded successfully." << endl;
    //     }

    //     // load the kernel

    //     // save the output image
    //     int saveImageFail = lspImageSave(opt.out_path.c_str(), image);

    //     if (saveImageFail)
    //     {
    //         cout << "Error saving image to path " << opt.out_path << endl;
    //         return;
    //     }
    //     else
    //     {
    //         cout << "Image has been saved to " << opt.out_path << " successfully." << endl;
    //     }

        // // load the image as nrrd
        // Nrrd *nin = nrrdNew();
        // airMopAdd(mop, nin, (airMopper)nrrdNuke, airMopAlways);
        // if (nrrdLoad(nin, opt.image_path.c_str(), NULL)) 
        // {
        //     printf("%s: trouble reading file\n", __func__);
        //     airMopError(mop);
        //     return;
        // }

        // // save the nrrd
        // Nrrd *nout = nin;
        // airMopAdd(mop, nout, (airMopper)nrrdNuke, airMopAlways);
        // if (nrrdSave(opt.out_path.c_str(), nout, NULL)) 
        // {
        //     printf("%s: trouble saving output\n", __func__);
        //     airMopError(mop);
        //     return;
        // }
        
        airMopOkay(mop);
        // lspCtx* ctx = mprCtxNew(lspImg, mprKernelBox);
}