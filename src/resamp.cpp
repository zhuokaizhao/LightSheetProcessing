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

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

#include <chrono>

using namespace std;
namespace fs = boost::filesystem;

void setup_resamp(CLI::App &app)
{
    auto opt = std::make_shared<resampOptions>();
    auto sub = app.add_subcommand("resamp", "Perform resampling on input images");

    sub->add_option("-i, --nhdr_path", opt->nhdr_path, "Path of input nrrd header files.")->required();
    sub->add_option("-g, --kernel", opt->grid_path, "Path that includes the grid file")->required();
    sub->add_option("-o, --out_path", opt->out_path, "Path that includes all the output images")->required();
    sub->add_option("-n, --max_file_number", opt->maxFileNum, "The max number of files that we want to process");
    sub->add_option("-v, --verbose", opt->verbose, "Print processing message or not. (Default: 0(close))");

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

void ConvoEval2D(lspCtx2D *ctx2D, double xw, double yw, airArray* mop) 
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

    NrrdKernelSpec* kernelSpec;
    kernelSpec = nrrdKernelSpecNew();
    airMopAdd(mop, kernelSpec, (airMopper)nrrdKernelSpecNix, airMopAlways);
    nrrdKernelParse(&(kernelSpec->kernel), kernelSpec->parm, ctx2D->kern->name);

    // get the support of kernel
    int support = (int)(kernelSpec->kernel->support(kernelSpec->parm));
    
    // even kernel
    if ( isEven(support) )
    {
        // n1 = floor(x1), n2 = floor(x2)
        n1 = floor(ctx2D->ipos[0]);
        n2 = floor(ctx2D->ipos[1]);
        // lower = 1 - support/2
        lower = 1 - support / 2;
        // upper = support/2
        upper = support / 2;
    }
    // odd kernel
    else
    {
        // n1 = floor(x1+0.5), n2 = floor(x2+0.5)
        n1 = floor(ctx2D->ipos[0] + 0.5);
        n2 = floor(ctx2D->ipos[1] + 0.5);
        // lower = (1 - support)/2
        lower = (1 - support) / 2;
        // upper = (support - 1)/2
        upper = (support - 1) / 2;
    }

    // calculate alpha based on n1, n2
    double alpha1, alpha2;
    alpha1 = ctx2D->ipos[0] - n1;
    alpha2 = ctx2D->ipos[1] - n2;

    // separable convolution
    double sum = 0;

    // initialize kernels
    double k1[support], k2[support];

    // precompute two vectors of kernel evaluations to save time
    for (int i = lower; i <= upper; i++)
    {
        k1[i - lower] = kernelSpec->kernel->eval1_d(alpha1 - i, kernelSpec->parm);
        k2[i - lower] = kernelSpec->kernel->eval1_d(alpha2 - i, kernelSpec->parm);
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
                }
            }

            // if no outside, assign value
            ctx2D->value = sum;
        }
    }

    return;
}

// function that performs 2D resampling
Nrrd* nrrdResample2D(Nrrd* nin, uint axis, NrrdKernel* kernel, double* kparm, airArray* mop)
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
    nrrdResampleRenormalizeSet(resampContext, AIR_TRUE);

    // start the resampling
    nrrdResampleExecute(resampContext, nout);

    return nout;

}

void ConvoEval3D(lspCtx3D *ctx3D, double xw, double yw, double zw, airArray* mop)
{
    // initialize output
    ctx3D->wpos[0] = xw;
    ctx3D->wpos[1] = yw;
    ctx3D->wpos[2] = zw;
    ctx3D->value[0] = lspNan(0);
    ctx3D->value[1] = lspNan(0);

    // first convert wpos to ipos, where ipos are x1, x2 and x3 as in FSV
    // MV4_MUL only takes 4-vector
    double ipos[4];
    double wpos[4] = {ctx3D->wpos[0], ctx3D->wpos[1], ctx3D->wpos[2], 1};

    // 4-vector ipos = 4x4 matrix WtoI * 4-vector wpos
    MV4_MUL(ipos, ctx3D->WtoI, wpos);
    // cout << "Corresponding index space in old Volume is (" << ipos[0] << ", " << ipos[1] << ", " << ipos[2] << ")" << endl;

    // set this to ctx
    ctx3D->ipos[0] = ipos[0];
    ctx3D->ipos[1] = ipos[1];
    ctx3D->ipos[2] = ipos[2];

    // parse the kernel
    NrrdKernelSpec* kernelSpec;
    kernelSpec = nrrdKernelSpecNew();
    airMopAdd(mop, kernelSpec, (airMopper)nrrdKernelSpecNix, airMopAlways);
    nrrdKernelParse(&(kernelSpec->kernel), kernelSpec->parm, ctx3D->kern->name);

    // get the support of kernel
    int support = (int)(kernelSpec->kernel->support(kernelSpec->parm));
    // cout << "Kernel support is " << support << endl;

    // determine different n1, n2 and n3 for even and odd kernels
    int n1, n2, n3;
    // determine lower and upper bounds for later convolution
    int lower, upper;

    // even kernel
    if ( isEven(support) )
    {
        // cout << "Even support" << endl;
        // n1 = floor(x1), n2 = floor(x2), n3 = floor(n3)
        n1 = floor(ctx3D->ipos[0]);
        n2 = floor(ctx3D->ipos[1]);
        n3 = floor(ctx3D->ipos[2]);
        // lower = 1 - support/2
        lower = 1 - support / 2;
        // upper = support/2
        upper = support / 2;
    }
    // odd kernel
    else
    {
        // cout << "Odd support" << endl;
        // n1 = floor(x1+0.5), n2 = floor(x2+0.5), n3 = floor(x3+0.5)
        n1 = floor(ctx3D->ipos[0] + 0.5);
        n2 = floor(ctx3D->ipos[1] + 0.5);
        n3 = floor(ctx3D->ipos[2] + 0.5);
        // lower = (1 - support)/2
        lower = (1 - support) / 2;
        // upper = (support - 1)/2
        upper = (support - 1) / 2;
    }

    for (int i = lower; i <= upper; i++)
    {
        if ( (i + n1 < 0 || i + n1 >= (int)ctx3D->volume->size[0])
            || (i + n2 < 0 || i + n2 >= (int)ctx3D->volume->size[1])
            || (i + n3 < 0 || i + n3 >= (int)ctx3D->volume->size[2]) )
        {
            // cout << "(" << i1 << ", " << i2 << ", " << i3 << ")" << " is outside" << endl;
            ctx3D->inside = 0;
            cout << "outside" << endl;
        }
    }

    // cout << "n1 is " << n1 << endl;
    // cout << "n2 is " << n2 << endl;
    // cout << "n3 is " << n3 << endl;
    // cout << "lower is " << lower << endl;
    // cout << "upper is " << upper << endl;

    // do all these only when inside
    if ( ctx3D->volume != NULL && ctx3D->inside == 1 )
    {
        // calculate alpha based on n1, n2 and n3
        double alpha1, alpha2, alpha3;
        alpha1 = ctx3D->ipos[0] - n1;
        alpha2 = ctx3D->ipos[1] - n2;
        alpha3 = ctx3D->ipos[2] - n3;
        // cout << "alpha1 is " << alpha1 << endl;
        // cout << "alpha2 is " << alpha2 << endl;
        // cout << "alpha3 is " << alpha3 << endl;

        // separable convolution for each channel, initialize to be all 0
        double sum[ctx3D->volume->channel] = {0};

        // initialize kernels
        double k1[support], k2[support], k3[support];

        // precompute three vectors of kernel evaluations to save time
        for (int i = lower; i <= upper; i++)
        {
            k1[i - lower] = kernelSpec->kernel->eval1_d(alpha1 - i, kernelSpec->parm);
            k2[i - lower] = kernelSpec->kernel->eval1_d(alpha2 - i, kernelSpec->parm);
            k3[i - lower] = kernelSpec->kernel->eval1_d(alpha3 - i, kernelSpec->parm);
        }
        // cout << "kernel values between range " << lower << " and " << upper << " have been pre-computed" << endl;
        // cout << "volume size 0 is " << ctx3D->volume->size[0] << endl;
        // cout << "volume size 1 is " << ctx3D->volume->size[1] << endl;
        // cout << "volume size 2 is " << ctx3D->volume->size[2] << endl;

        // compute via three nested loops over the 3D-kernel support
        // faster axis first (i1 fast)
        for (int i3 = lower; i3 <= upper; i3++)
        {
            for (int i2 = lower; i2 <= upper; i2++)
            {
                for (int i1 = lower; i1 <= upper; i1++)
                {   
                    // // we are inside, and we have two channels
                    for (int c = 0; c < ctx3D->volume->channel; c++)
                    {
                        // compute data index
                        uint data_index = (i3+n3)*(ctx3D->volume->size[1]*ctx3D->volume->size[0]) + (i2+n2)*(ctx3D->volume->size[0]) + n1 + i1 + c;
                        // cout << "current data_index is " << data_index << endl;

                        // we do have different types of inputs
                        // we converted unsigned short to short
                        if (ctx3D->volume->dtype == lspTypeShort || ctx3D->volume->dtype == lspTypeUShort)
                        {
                            sum[c] = sum[c] + ctx3D->volume->data.s[data_index] * k1[i1-lower] * k2[i2-lower] * k3[i3-lower];
                        }
                        else if (ctx3D->volume->dtype == lspTypeDouble)
                        {
                            sum[c] = sum[c] + ctx3D->volume->data.dl[data_index] * k1[i1-lower] * k2[i2-lower] * k3[i3-lower];
                        }
                        else
                        {
                            cout << "ConvoEval3D: unknown data type" << endl;
                            return;
                        }
                        
                        // cout << "sum is " << sum[c] << endl;
                    }
                }
            }
        }

        // assign value
        for (int c = 0; c < ctx3D->volume->channel; c++)
        {
            ctx3D->value[c] = sum[c];
        }
    }

    return; 

}

// function that performs 3D resampling (convolution)
int nrrdResample3D(lspVolume* newVolume, lspCtx3D* ctx3D, airArray* mop)
{   
    // sizes in x, y and z directions
    uint sizeX = ctx3D->boundaries[0];
    uint sizeY = ctx3D->boundaries[1];
    uint sizeZ = ctx3D->boundaries[2];

    // evaluate at each new volume index-space position
    for (uint zi = 0; zi < sizeZ; zi++)
    {
        double percentage = 100.0*(double)zi/(double)sizeZ;
        cout << percentage << " percents of the volume have been done" << endl;
        for (uint yi = 0; yi < sizeY; yi++)
        {
            for (uint xi = 0; xi < sizeX; xi++)
            {
                // convert the new-volume index space to world
                uint new_ipos[4] = {xi, yi, zi, 1};
                // cout << "Current newVolume index space is (" << xi << ", " << yi << ", " << zi << ")" << endl;
                double wpos[4];
                MV4_MUL(wpos, ctx3D->NewItoW, new_ipos);
                // cout << "Corresponding world space is (" << wpos[0] << ", " << wpos[1] << ", " << wpos[2] << ")" << endl;
                ConvoEval3D(ctx3D, wpos[0], wpos[1], wpos[2], mop);
                // cout << "Finished evaluating at new volume index space (" << xi << ", " << yi << ", " << zi << ")" << endl;
                for (int c = 0; c < ctx3D->volume->channel; c++)
                {
                    if (ctx3D->volume->dtype == lspTypeShort || ctx3D->volume->dtype == lspTypeUShort)
                    {
                        newVolume->data.s[c + ctx3D->volume->channel*(xi + sizeX*(yi + sizeY*zi))] = ctx3D->value[c];
                    }
                    else if (ctx3D->volume->dtype == lspTypeDouble)
                    {
                        newVolume->data.dl[c + ctx3D->volume->channel*(xi + sizeX*(yi + sizeY*zi))] = ctx3D->value[c];
                    }
                    else
                    {
                        cout << "nrrdResample3D: error assigning convolution result to the new volume, unknown data type" << endl;
                        return 1;
                    }
                }
            }
        }
    }

    return 0;
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
        cout << "Finish loading Nrrd data located at " << nhdr_name << endl;

        // do the permutation
        // permute from x(0)-y(1)-channel(2)-z(3) to channel(2)-x(0)-y(1)-z(3)
        unsigned int permute[4] = {2, 0, 1, 3};
        Nrrd* nin_permuted = safe_nrrd_new(mop, (airMopper)nrrdNuke);
        if ( nrrdAxesPermute(nin_permuted, nin, permute) )
        {
            cout << "nrrdAxesPermute failed, program stops" << endl;
            return;
        }
        cout << "Finished permutation" << endl;

        // put Nrrd data into lspVolume
        lspVolume* volume = lspVolumeNew();
        airMopAdd(mop, volume, (airMopper)lspVolumeNix, airMopAlways);
        if ( lspVolumeFromNrrd(volume, nin_permuted) )
        {
            cout << "lspVolumeFromNrrd failed, program stops" << endl;
            return;
        }
        cout << "Finished converting Nrrd data to lspVolume" << endl;

        // put both volume and box kernel into the Ctx3D
        lspCtx3D* ctxBox = lspCtx3DNew(volume, opt.grid_path, nrrdKernelBox, NULL);
        cout << "Finished generating ctxBox container" << endl;

        // perform the 3D sampling (convolution)
        // resulting volume_box
        lspVolume* volume_new = lspVolumeNew();
        airMopAdd(mop, volume_new, (airMopper)lspVolumeNix, airMopAlways);
        // allocate memory for the new volume, with sizes being the region of interest sizes
        if (lspVolumeAlloc(volume_new, volume->channel, ctxBox->boundaries[0], ctxBox->boundaries[1], ctxBox->boundaries[2], volume->dtype)) 
        {
            printf("%s: trouble allocating volume\n", __func__);
            return;
        }

        // start 3D convolution
        if ( nrrdResample3D(volume_new, ctxBox, mop) )
        {
            printf("%s: trouble computing 3D convolution\n", __func__);
            return;
        }
        cout << "Finished resampling with Box kernel" << endl;

        // // Catmull-Rom kernel, which has 4 sample support and is 2-accurate
        // // put both volume_box and ctmr kernel into the Ctx3D
        // lspCtx3D* ctxCtmr = lspCtx3DNew(volume_box, opt.grid_path, nrrdKernelCatmullRom, NULL /* imm */);

        // // perform the 3D sampling (convolution)
        // // resulting volume_ctmr
        // lspVolume* volume_new = lspVolumeNew();
        // allocate memory for the new volume, with sizes being the region of interest sizes
        // if (lspVolumeAlloc(volume_new, volume->channel, ctxCtmr->boundaries[0], ctxCtmr->boundaries[1], ctxCtmr->boundaries[2], volume->dtype)) 
        // {
        //     printf("%s: trouble allocating volume\n", __func__);
        //     return;
        // }
        // airMopAdd(mop, volume_new, (airMopper)lspVolumeNix, airMopAlways);
        // if ( nrrdResample3D(volume_new, ctxCtmr, mop) )
        // {
        //     printf("%s: trouble computing 3D convolution\n", __func__);
        //     return;
        // }
        // cout << "Finished resampling with CatmullRom kernel" << endl;

        // change the volume back to Nrrd file for projection
        Nrrd* nout = safe_nrrd_new(mop, (airMopper)nrrdNuke);
        lspNrrdFromVolume(nout, volume_new);
        cout << "Finished converting resulting volume back to Nrrd data" << endl;

        // Project the volume alone z axis using MIP
        Nrrd* projNrrd = safe_nrrd_new(mop, (airMopper)nrrdNuke);
        nrrdProject(projNrrd, nout, 3, nrrdMeasureMax, nrrdTypeDouble);
        cout << "Finished projecting Nrrd data alone z-axis" << endl;
        
        // slice the nrrd into separate GFP and RFP channel (and quantize to 8bit)
        Nrrd* slices[2] = {safe_nrrd_new(mop, (airMopper)nrrdNuke), 
                            safe_nrrd_new(mop, (airMopper)nrrdNuke)};
        cout << "Finished slicing the data based on its channel (GFP and RFP)" << endl;

        // quantized
        Nrrd* quantized[2] = {safe_nrrd_new(mop, (airMopper)nrrdNuke),
                                safe_nrrd_new(mop, (airMopper)nrrdNuke)};

        // range during quantizing
        auto range = nrrdRangeNew(lspNan(0), lspNan(0));
        airMopAdd(mop, range, (airMopper)nrrdRangeNix, airMopAlways);

        for (int i = 0; i < 2; i++)
        {
            nrrdSlice(slices[i], projNrrd, 0, i);
            nrrdRangePercentileFromStringSet(range, slices[i],  "0.1%", "0.1%", 5000, true);
            nrrdQuantize(quantized[i], slices[i], range, 8);
        }
        cout << "Finished quantizing to 8-bit" << endl;

        // Join the two channel
        Nrrd* finalJoined = safe_nrrd_new(mop, (airMopper)nrrdNuke);
        nrrdJoin(finalJoined, quantized, 2, 0, 1);
        cout << "Fnished joining the 2 channels" << endl;

        // save the final nrrd as image
        if (nrrdSave(opt.out_path.c_str(), finalJoined, NULL)) 
        {
            printf("%s: trouble saving output\n", __func__);
            airMopError(mop);
        }

        cout << "Finished saving image at " << opt.out_path << endl;

    }
    
        
    airMopOkay(mop);
    return;


}