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
    sub->add_option("-m, --mode", opt->mode, "<VideoOnly> for generating video only (no resampling will happen), can be empty, default will do");
    sub->add_option("-g, --grid", opt->grid_path, "Path that includes the grid file")->required();
    sub->add_option("-k, --kernel", opt->kernel_name, "Name of the kernel. Currently support 'box' and 'ctml'.")->required();
    sub->add_option("-o, --out_path", opt->out_path, "Path that includes all the output images")->required();
    sub->add_option("-n, --max_file_number", opt->maxFileNum, "The max number of files that we want to process");
    sub->add_option("-f, --fps", opt->fps, "Frame per second (fps) of the generated .avi video. (Default: 10)");
    sub->add_option("-v, --verbose", opt->verbose, "Print processing message or not. (Default: 0(close))");

    sub->set_callback([opt]() 
    {
        // first determine if input nhdr_path is valid
        if (checkIfDirectory(opt->nhdr_path))
        {
            opt->isSingleFile = false;

            cout << "nhdr input directory " << opt->nhdr_path << " is valid" << endl;
            
            // count the number of files
            const vector<string> files = GetDirectoryFiles(opt->nhdr_path);

            // count the number of valid nhdr files
            int nhdrNum = 0;
            
            for (int i = 0; i < files.size(); i++) 
            {
                string curFile = files[i];
                // check if input file is a .nhdr file
                int nhdr_suff = curFile.rfind(".nhdr");
                if ( (nhdr_suff != string::npos) && (nhdr_suff == curFile.length() - 5) )
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
                        if (curFileName[i] != '0')
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
                        // curFileName is like 001, without the file type
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
                cout << "User restricts the total number of files to be processed as " << nhdrNum << endl;
            }
            opt->numFiles = nhdrNum;
            cout << "Total number of .nhdr files that we are processing is: " << opt->numFiles << endl << endl;

            try
            {
                // cout << "First file name is " << opt->allValidFiles[0].second << endl;
                // we are generating these files
                auto start = chrono::high_resolution_clock::now();
                Resamp(*opt).main();
                auto stop = chrono::high_resolution_clock::now(); 
                auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 
                cout << endl << "Processed " << nhdrNum << " files took " << duration.count() << " seconds" << endl << endl; 
                // make video
                Resamp(*opt).makeVideo();
            }
            catch(LSPException &e)
            {
                std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
            }
        }
        // check if we can get to single-file mode
        else
        {
            // the program also handles if input file is a single file
            opt->isSingleFile = true;
            const string curFile = opt->nhdr_path;
            cout << curFile << " is not a directory, check if it is a valid .nhdr file" << endl;
            
            // check if input file is a .nhdr file
            int nhdr_suff = curFile.rfind(".nhdr");

            // if it is a valid file
            if ( (nhdr_suff != string::npos) && (nhdr_suff == curFile.length() - 5) )
            {
                cout << "Current input file " + curFile + " ends with .nhdr, process this file" << endl;
                opt->numFiles = 1;
                // we want to actually learn the name of the file with .nhdr
                int start = curFile.rfind("/");
                string curFileName = curFile.substr(start+1, 3);
                std::cout << "Current file name is: " << curFileName << endl;

                try
                {
                    // we will save this new volume as nrrd
                    string volumeOutPath = opt->out_path + "/" + curFileName + ".nhdr";

                    // we will save the final nrrd as image
                    string imageOutPath = opt->out_path + "/" + curFileName + ".png";

                    // when output already exists, skip this
                    if (fs::exists(volumeOutPath) && fs::exists(imageOutPath))
                    {
                        cout << volumeOutPath << " and " << imageOutPath << " both exists, continue to next." << endl;
                        return;
                    }

                    // we are computing these files
                    auto start = chrono::high_resolution_clock::now();
                    Resamp(*opt).main();
                    auto stop = chrono::high_resolution_clock::now(); 
                    auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 
                    cout << endl << "Processed " << opt->nhdr_path << " took " << duration.count() << " seconds" << endl << endl; 
                    // we don't make video here since it is just a single image
                }
                catch(LSPException &e)
                {
                    std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
                }
            }
            else
            {
                cout << "Current input file " + curFile + " does not end with .nhdr, error" << endl;
            }
            
        }
    });
}

Resamp::Resamp(resampOptions const &opt): opt(opt), mop(airMopNew())
{
    // create folder if it does not exist (only when it is not in single file mode)
    if (!opt.isSingleFile)
    {
        if (!checkIfDirectory(opt.out_path))
        {
            boost::filesystem::create_directory(opt.out_path);
            cout << "Image output path " << opt.out_path << " does not exits, but has been created" << endl;
        }
    }
}

Resamp::~Resamp()
{
    // cout << "Line 201" << endl;
    airMopOkay(mop);
    // cout << "Line 203" << endl;
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

// helper function that does all the processing, created as helper function since there are two modes
static void processData(Nrrd* nrrd_new, string nhdr_name, string grid_path, string kernel_name, string volumeOutPath, airArray* mop, int verbose)
{
    // load the nhdr header
    Nrrd* nin = safe_nrrd_load(mop, nhdr_name);
    if (verbose)
    {
        cout << "Finish loading Nrrd data located at " << nhdr_name << endl;
    }

    // do the permutation
    // permute from x(0)-y(1)-channel(2)-z(3) to channel(2)-x(1)-y(2)-z(3)
    unsigned int permute[4] = {2, 0, 1, 3};
    Nrrd* nin_permuted = safe_nrrd_new(mop, (airMopper)nrrdNuke);
    if ( nrrdAxesPermute(nin_permuted, nin, permute) )
    {
        if (verbose)
        {
            cout << "nrrdAxesPermute failed, program stops" << endl;
        }
        return;
    }
    if (verbose)
    {
        cout << "Finished permutation" << endl;
    }

    // put Nrrd data into lspVolume
    lspVolume* volume = lspVolumeNew();
    // airMopAdd(mop, volume, (airMopper)lspVolumeNix, airMopAlways);

    if ( lspVolumeFromNrrd(volume, nin_permuted) )
    {
        if (verbose)
        {
            cout << "lspVolumeFromNrrd failed, program stops" << endl;
        }
        return;
    }
    if (verbose)
    {
        cout << "Finished converting Nrrd data to lspVolume" << endl;
    }

    // put both volume and user-defined kernel into the Ctx3D
    const NrrdKernel* kernel;
    if (kernel_name == "box")
    {
        kernel = nrrdKernelBox;
    }
    else if (kernel_name == "ctml")
    {
        kernel = nrrdKernelCatmullRom;
    }

    // get the container ready
    lspCtx3D* ctx = lspCtx3DNew(volume, grid_path, kernel, NULL);
    if (verbose)
    {
        cout << "Finished generating ctx container" << endl;
    }

    // perform the 3D sampling (convolution)
    lspVolume* volume_new = lspVolumeNew();
    // airMopAdd(mop, volume_new, (airMopper)lspVolumeNix, airMopAlways);

    // allocate memory for the new volume, with sizes being the region of interest sizes
    if (lspVolumeAlloc(volume_new, volume->channel, ctx->boundaries[0], ctx->boundaries[1], ctx->boundaries[2], volume->dtype)) 
    {
        if (verbose)
        {
            printf("%s: trouble allocating volume\n", __func__);
        }
        return;
    }

    // start 3D convolution
    if ( nrrdResample3D(volume_new, ctx) )
    {
        if (verbose)
        {
            printf("%s: trouble computing 3D convolution\n", __func__);
        }
        return;
    }
    if (verbose)
    {
        cout << "Finished resampling with Box kernel" << endl;
    }

    // change the volume back to Nrrd file for projection
    if (lspNrrdFromVolume(nrrd_new, volume_new))
    {
        if (verbose)
        {
            printf("%s: trouble converting Volume to Nrrd data\n", __func__);
        }
        return;
    }
    if (verbose)
    {
        cout << "Finished converting resulting volume back to Nrrd data" << endl;
    }

    // save the resampled data as NHDR files
    if (nrrdSave(volumeOutPath.c_str(), nrrd_new, NULL)) 
    {
        if (verbose)
        {
            printf("%s: trouble saving new volume as Nrrd file\n", __func__);
        }
        airMopError(mop);
        return;
    }
    cout << "Finished saving new volume at " << volumeOutPath << endl;

    // free memory
    lspVolumeNix(volume);
    lspVolumeNix(volume_new);
    lspCtx3DNix(ctx);
}

// helper function that crops the the input data set based on start and end percentage of along each axis
static void cropDataSet(Nrrd* nin_cropped, Nrrd* nin, double startPercent_x, double endPercent_x, double startPercent_y, double endPercent_y, double startPercent_z, double endPercent_z)
{
    size_t min[4], max[4];
    // first axis is the channel, not cropping here
    min[0] = 0;
    max[0] = nin->axis[0].size-1;
    // x axis
    min[1] = (size_t)floor( startPercent_x * (nin->axis[1].size-1) );
    max[1] = (size_t)floor( endPercent_x * (nin->axis[1].size-1) );
    // y axis
    min[2] = (size_t)floor( startPercent_y * (nin->axis[2].size-1) );
    max[2] = (size_t)floor(endPercent_y * (nin->axis[2].size-1) );
    // z axis
    min[3] = (size_t)floor( startPercent_z * (nin->axis[3].size-1) );
    max[3] = (size_t)floor( endPercent_z * (nin->axis[3].size-1) );

    // do the cropping
    nrrdCrop(nin_cropped, nin, min, max);
}

// function that project the "percent" of the loaded volume alone a specific axis
static void projectData(Nrrd* projNrrd, Nrrd* nin, string axis, double startPercent, double endPercent, int verbose, airArray* mop)
{
    // considering there are two channels, total dimension is 4
    size_t min[4], max[4];
    min[0] = 0;
    max[0] = nin->axis[0].size-1;
    int axisNum = lspNan(0);
    if (axis == "x")
    {
        axisNum = 1;
        // x-axis is what we are going to crop with
        min[1] = (size_t)floor( startPercent * (nin->axis[1].size-1) );
        max[1] = (size_t)floor( endPercent * (nin->axis[1].size-1) );
        min[2] = 0;
        max[2] = nin->axis[2].size-1;
        min[3] = 0;
        max[3] = nin->axis[3].size-1;
    }
    else if (axis == "y")
    {
        axisNum = 2;
        // y-axis is what we are going to crop with
        min[1] = 0;
        max[1] = nin->axis[1].size-1;
        min[2] = (size_t)floor( startPercent * (nin->axis[2].size-1) );
        max[2] = (size_t)floor(endPercent * (nin->axis[2].size-1) );
        min[3] = 0;
        max[3] = nin->axis[3].size-1;
    }
    else if (axis == "z")
    {
        axisNum = 3;
        // z-axis is what we are going to crop with
        min[1] = 0;
        max[1] = nin->axis[1].size-1;
        min[2] = 0;
        max[2] = nin->axis[2].size-1;
        min[3] = (size_t)floor( startPercent * (nin->axis[3].size-1) );
        max[3] = (size_t)floor( endPercent * (nin->axis[3].size-1) );
    }
    else
    {
        cout << "projectData: Unknown axis number, return" << endl;
        return;
    }
    // cout << "min is (" << min[0] << ", " << min[1] << ", " << min[2] << ", " << min[3] << ")" << endl;
    // cout << "max is (" << max[0] << ", " << max[1] << ", " << max[2] << ", " << max[3] << ")" << endl;

    // cropping takes place at the projected axis
    Nrrd* nin_cropped = safe_nrrd_new(mop, (airMopper)nrrdNuke);
    nrrdCrop(nin_cropped, nin, min, max);
    // cout << "nin_cropped has dimension ( ";
    // for (int i = 0; i < 4; i++)
    // {
    //     cout << nin_cropped->axis[i].size << " ";
    // }
    // cout << ")" << endl;
    

    // Project the loaded data alone input axis using MIP
    if (nrrdProject(projNrrd, nin_cropped, axisNum, nrrdMeasureMax, nrrdTypeDouble))
    {
        if (verbose)
        {
            printf("%s: trouble projecting Nrrd data alone x-axis using MIP\n", __func__);
        }
        airMopError(mop);
        return;
    }
    if (verbose)
    {
        cout << "Finished projecting Nrrd data alone x-axis" << endl;
    }
}

// function that generates range from input data for both GFP and RFP
static void generateRange(Nrrd* nin, NrrdRange* range_GFP, NrrdRange* range_RFP, const vector<string> rangeMinPercentile, const vector<string> rangeMaxPercentile, int verbose, airArray* mop)
{
    // projected Nrrd dataset
    Nrrd* projNrrd = safe_nrrd_new(mop, (airMopper)nrrdNuke);
    // make the projection alone input axis
    projectData(projNrrd, nin, "z", 0.0, 1.0, verbose, mop);

    // slice the nrrd into separate GFP and RFP channel (axis 0) (and quantize to 8bit)
    Nrrd* slices[2] = {safe_nrrd_new(mop, (airMopper)nrrdNuke), 
                        safe_nrrd_new(mop, (airMopper)nrrdNuke)};

    // slice the data based into separate channels
    for (int i = 0; i < 2; i++)
    {
        if (nrrdSlice(slices[i], projNrrd, 0, i))
        {
            if (verbose)
            {
                printf("%s: trouble slicing into 2 channels projected alone z axis\n", __func__);
            }
            airMopError(mop);
            return;
        }
        if (verbose)
        {
            printf("%s: Finished slicing the data based on its channel (GFP and RFP) projected alone z axis\n", __func__);
        }
    }

    if (nrrdRangePercentileFromStringSet(range_GFP, slices[0],  rangeMinPercentile[0].c_str(), rangeMaxPercentile[0].c_str(), 5000, true)
        || nrrdRangePercentileFromStringSet(range_RFP, slices[1],  rangeMinPercentile[1].c_str(), rangeMaxPercentile[1].c_str(), 5000, true))
    {
        printf("%s: trouble generating ranges for GFP and RFP\n", __func__);
    }
    if (verbose)
    {
        cout << "GFP min is " << range_GFP->min << ", GFP max is " << range_GFP->max << endl;
        cout << "RFP min is " << range_RFP->min << ", RFP max is " << range_RFP->max << endl;
    }
}

// generating projection image alone the input axis
static void makeProjImage(Nrrd* nin, string axis, double startPercent, double endPercent, string imageOutPath, NrrdRange* range_GFP, NrrdRange* range_RFP, int verbose, airArray* mop)
{
    // projected Nrrd dataset
    Nrrd* projNrrd = safe_nrrd_new(mop, (airMopper)nrrdNuke);
    // make the projection alone input axis
    projectData(projNrrd, nin, axis, startPercent, endPercent, verbose, mop);

    // slice the nrrd into separate GFP and RFP channel (axis 0) (and quantize to 8bit)
    Nrrd* slices[2] = {safe_nrrd_new(mop, (airMopper)nrrdNuke), 
                        safe_nrrd_new(mop, (airMopper)nrrdNuke)};
    // quantized to 8-bit
    Nrrd* quantized[2] = {safe_nrrd_new(mop, (airMopper)nrrdNuke),
                            safe_nrrd_new(mop, (airMopper)nrrdNuke)};

    for (int i = 0; i < 2; i++)
    {
        if (nrrdSlice(slices[i], projNrrd, 0, i))
        {
            if (verbose)
            {
                printf("%s: trouble slicing into 2 channels projected alone %s axis\n", __func__, axis);
            }
            airMopError(mop);
            return;
        }
        if (verbose)
        {
            cout << "Finished slicing the data based on its channel (GFP and RFP) projected alone " << axis << " axis" << endl;
        }
    }
    // range should be pre-defined elsewhere and passed as input, return if empty
    if (!range_GFP->min || !range_GFP->max || !range_RFP->min || !range_RFP->max)
    {
        printf("%s: range for projection should've been passed as input\n", __func__, axis);
        return;
    }
    if (verbose)
    {
        cout << "X projection GFP min is " << range_GFP->min << ", GFP max is " << range_GFP->max << endl;
        cout << "X projection RFP min is " << range_RFP->min << ", RFP max is " << range_RFP->max << endl;
    }

    vector<NrrdRange*> range;
    range.push_back(range_GFP);
    range.push_back(range_RFP);

    for (int i = 0; i < 2; i++)
    {
        if (nrrdQuantize(quantized[i], slices[i], range[i], 8))
        {
            if (verbose)
            {
                printf("%s: trouble quantizing to 8 bits projected alone %s axis\n", __func__, axis);
            }
            airMopError(mop);
            return;
        }
        if (verbose)
        {
            cout << "Finished quantizing to 8-bit projected alone " << axis << " axis" << endl;
        }
    }

    // final joined nrrd data file
    Nrrd* finalJoined = safe_nrrd_new(mop, (airMopper)nrrdNuke);
    // Join the two channel
    if (nrrdJoin(finalJoined, quantized, 2, 0, 1))
    {
        if (verbose)
        {
            printf("%s: trouble joining 2 channels projected in z plane\n", __func__);
        }
        airMopError(mop);
    }
    if (verbose)
    {
        cout << "Fnished joining the 2 channels projected in z plane" << endl;
    }

    // right now it is two channel png [GFP RFP], we want to make it a three channel [RFP GFP RFP]
    // unu pad -i 598.png -min -1 0 0 -max M M M -b wrap -o tmp.png
    ptrdiff_t min[3] = {-1, 0, 0};
    ptrdiff_t max[3] = {(ptrdiff_t)finalJoined->axis[0].size-1, (ptrdiff_t)finalJoined->axis[1].size-1, (ptrdiff_t)finalJoined->axis[2].size-1};
    Nrrd* finalPaded = safe_nrrd_new(mop, (airMopper)nrrdNuke);
    nrrdPad_va(finalPaded, finalJoined, min, max, nrrdBoundaryWrap);

    if (nrrdSave(imageOutPath.c_str(), finalPaded, NULL)) 
    {
        if (verbose)
        {
            printf("%s: trouble saving output\n", __func__);
        }
        airMopError(mop);
    }
    cout << "Finished saving image at " << imageOutPath << endl;
    if (verbose)
    {
        cout << imageOutPath << " has dimension ( ";
        for (int i = 0; i < 3; i++)
        {
            cout << finalPaded->axis[i].size << " ";
        }
        cout << ")" << endl;
    }
}

// helper function that perform image denoising
static cv::Mat3b textureRemoval(cv::Mat3b inputImage)
{
    cv::Mat3b outputImage;
    cv::fastNlMeansDenoisingColored(inputImage, outputImage, 3, 7, 21);

    return outputImage;
}

// helper function that stitches left, middle and right image
static void stitchImages(string imageOutPath_x_left, string imageOutPath_z, string imageOutPath_x_right, string common_prefix)
{
    // use OpenCV to join two images
    // Load images
    cv::Mat3b img_x_left = cv::imread(imageOutPath_x_left);
    cv::Mat3b img_z = cv::imread(imageOutPath_z);
    cv::Mat3b img_x_right = cv::imread(imageOutPath_x_right);

    // flip and rotate the x-projected left image by 90 degrees counterclock-wise
    cv::flip(img_x_left, img_x_left, 1);
    cv::transpose(img_x_left, img_x_left);
    cv::flip(img_x_left, img_x_left, 0);
    // rotate x-projected right image by 90 degrees clock-wise
    cv::transpose(img_x_right, img_x_right);
    cv::flip(img_x_right, img_x_right, 1);

    // Get dimension of final image
    // 616 rows (y direction top to bottom)
    int rows = cv::max(img_z.rows, img_x_left.rows);
    // 550+616 columes (x direction left to right)
    int cols = img_z.cols + img_x_left.cols + img_x_right.cols;

    // Create a black image
    cv::Mat3b res(rows, cols, cv::Vec3b(0,0,0));

    // Copy images in correct position
    img_x_left.copyTo(res(cv::Rect(0, 0, img_x_left.cols, img_x_left.rows)));
    img_z.copyTo(res(cv::Rect(img_x_left.cols, 0, img_z.cols, img_z.rows)));
    img_x_right.copyTo(res(cv::Rect(img_x_left.cols + img_z.cols, 0, img_x_right.cols, img_x_right.rows)));

    // rotate the joined image by 180 degrees clock-wise
    cv::transpose(res, res);
    cv::flip(res, res, 1);
    cv::transpose(res, res);
    cv::flip(res, res, 1);

    // texture removal
    cv::Mat3b res_removed = textureRemoval(res);

    // Show result
    string imageOutPath_joined = common_prefix + "_joined.png";
    cv::imwrite(imageOutPath_joined, res_removed);
    cout << "Finished saving stitched image to " << imageOutPath_joined << endl;
}

// ********************** end of static helper functions *********************

void convoEval3D(lspCtx3D *ctx3D, double xw, double yw, double zw)
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

    // cout << "Kernel support is " << ctx3d->support << endl;
    int support = (int)(ctx3D->kernelSpec->kernel->support(ctx3D->kernelSpec->parm));
    // cout << "support is " << support << endl;
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
            // cout << "outside" << endl;
        }
    }

    // do all these only when inside
    if ( ctx3D->volume != NULL && ctx3D->inside == 1 )
    {
        // calculate alpha based on n1, n2 and n3
        double alpha1, alpha2, alpha3;
        alpha1 = ctx3D->ipos[0] - n1;
        alpha2 = ctx3D->ipos[1] - n2;
        alpha3 = ctx3D->ipos[2] - n3;

        // separable convolution for each channel, initialize to be all 0
        double sum[ctx3D->volume->channel] = {0};

        // initialize kernels
        double k1[support], k2[support], k3[support];

        // precompute three vectors of kernel evaluations to save time
        for (int i = lower; i <= upper; i++)
        {
            k1[i - lower] = ctx3D->kernelSpec->kernel->eval1_d(alpha1 - i, ctx3D->kernelSpec->parm);
            k2[i - lower] = ctx3D->kernelSpec->kernel->eval1_d(alpha2 - i, ctx3D->kernelSpec->parm);
            k3[i - lower] = ctx3D->kernelSpec->kernel->eval1_d(alpha3 - i, ctx3D->kernelSpec->parm);
        }

        // compute via three nested loops over the 3D-kernel support
        // faster axis first (i1 fast)
        uint channel = ctx3D->volume->channel;
        uint sizeX = ctx3D->volume->size[0];
        uint sizeY = ctx3D->volume->size[1];
        uint sizeZ = ctx3D->volume->size[2];

        for (int i3 = lower; i3 <= upper; i3++)
        {
            for (int i2 = lower; i2 <= upper; i2++)
            {
                for (int i1 = lower; i1 <= upper; i1++)
                {   
                    // // we are inside, and we have two channels
                    for (int c = 0; c < channel; c++)
                    {
                        // compute data index (4D stripe)
                        uint data_index = c + channel * ( (n1+i1) + sizeX * ( (n2+i2) + sizeY * (n3+i3) ) );
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
                    }
                }
            }
        }

        // assign value
        for (int c = 0; c < channel; c++)
        {
            ctx3D->value[c] = sum[c];
        }
    }

    return; 

}

// function that performs 3D resampling (convolution)
int nrrdResample3D(lspVolume* newVolume, lspCtx3D* ctx3D)
{   
    // sizes in x, y and z directions
    uint channel = ctx3D->volume->channel;
    uint sizeX = ctx3D->boundaries[0];
    uint sizeY = ctx3D->boundaries[1];
    uint sizeZ = ctx3D->boundaries[2];

    // evaluate at each new volume index-space position
    for (uint zi = 0; zi < sizeZ; zi++)
    {
        double percentage = 100.0*(double)zi/(double)sizeZ;
        // cout << percentage << " percents of the volume have been done" << endl;

        for (uint yi = 0; yi < sizeY; yi++)
        {
            for (uint xi = 0; xi < sizeX; xi++)
            {
                // convert the new-volume index space to world
                uint new_ipos[4] = {xi, yi, zi, 1};

                double wpos[4];
                MV4_MUL(wpos, ctx3D->NewItoW, new_ipos);

                convoEval3D(ctx3D, wpos[0], wpos[1], wpos[2]);

                for (int c = 0; c < ctx3D->volume->channel; c++)
                {
                    uint data_index = c + channel * ( xi + sizeX * ( yi + sizeY * zi ) );

                    if (ctx3D->volume->dtype == lspTypeShort || ctx3D->volume->dtype == lspTypeUShort)
                    {
                        newVolume->data.s[data_index] = ctx3D->value[c];
                    }
                    else if (ctx3D->volume->dtype == lspTypeDouble)
                    {
                        newVolume->data.dl[data_index] = ctx3D->value[c];
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
    // loop over all the current files
    // in single file mode, numFiles = 1, the for loop just runs once
    int nhdrNum = opt.numFiles;
    for (int i = 0; i < nhdrNum; i++)
    {
        auto start = chrono::high_resolution_clock::now();
        string common_prefix, volumeOutPath, imageOutPath_x_left, imageOutPath_x_right, imageOutPath_z, nhdr_name, curFileName;
        int startlocation;
        // non single file mode
        if (!opt.isSingleFile)
        {
            common_prefix = opt.out_path + "/" + opt.allValidFiles[i].second;
            // we will save this volume as nrrd
            volumeOutPath;

            // the final images's x and z component
            imageOutPath_x_left = common_prefix + "_x_left.png";
            imageOutPath_x_right = common_prefix + "_x_right.png";
            imageOutPath_z = common_prefix + "_z.png";

            nhdr_name = opt.nhdr_path + opt.allValidFiles[i].second + ".nhdr";
            cout << "Currently processing input file " << nhdr_name << endl;
        }
        // single file mode
        else
        {
            // since it is single file mode, nhdr_path is now the file path
            nhdr_name = opt.nhdr_path;
            // get the name of the file only
            startlocation = nhdr_name.rfind("/");
            curFileName = nhdr_name.substr(startlocation+1, 3);

            // we will save this new volume as nrrd
            common_prefix = opt.out_path + "/" + curFileName;
            volumeOutPath;
            // the final images's x and z component
            imageOutPath_x_left = common_prefix + "_x_left.png";
            imageOutPath_x_right = common_prefix + "_x_right.png";
            imageOutPath_z = common_prefix + "_z.png";   
        }

        // nrrd_new is the processed(resampled) new nrrd data
        Nrrd* nin;
        // when we are not in VideoOnly mode, we need to process the input data
        if (opt.mode.empty() || opt.mode != "VideoOnly")
        {
            // non video only mode
            cout << "Non-Single File Non-Video only mode, found " << nhdrNum << " processed .nhdr files" << endl;
            // when output already exists, skip this iteration
            if (fs::exists(volumeOutPath) && fs::exists(imageOutPath_x_left)
                && fs::exists(imageOutPath_x_right) && fs::exists(imageOutPath_z))
            {
                cout << "All outputs exist, continue to next." << endl;
                continue;
            }

            // the processed data will be put in nin
            nin = safe_nrrd_new(mop, (airMopper)nrrdNuke);
            // we will save this volume as nrrd
            volumeOutPath = common_prefix + ".nhdr";
            processData(nin, nhdr_name, opt.grid_path, opt.kernel_name, volumeOutPath, mop, opt.verbose);
        }
        // video only mode
        else
        {
            cout << "Non-Single File Video only mode, found " << nhdrNum << " processed .nhdr files" << endl;

            // when output already exists, skip this iteration
            if (fs::exists(imageOutPath_x_left) && fs::exists(imageOutPath_x_right) && fs::exists(imageOutPath_z))
            {
                cout << imageOutPath_x_left << ", " << imageOutPath_x_right << " and " << imageOutPath_z << " all exist, continue to next." << endl;
                continue;
            }

            nin = safe_nrrd_load(mop, nhdr_name);
            if (opt.verbose)
            {
                cout << "Finish loading Nrrd data located at " << nhdr_name << endl;
            }
        }

        // we want to get the RFP data range from the middle part of the data set, so that the
        // pioneers which have low brightness would not be influenced by the
        // so we take the center part of the data first
        Nrrd* nin_cropped = safe_nrrd_new(mop, (airMopper)nrrdNuke);
        // range of cropping along each axis
        double startPercent_x = 0.25;
        double endPercent_x = 0.75;
        double startPercent_y = 0.25;
        double endPercent_y = 0.75;
        double startPercent_z = 0.0;
        double endPercent_z = 1.0;
        cropDataSet(nin_cropped, nin, startPercent_x, endPercent_x, startPercent_y, endPercent_y, startPercent_z, endPercent_z);
        if (opt.verbose)
        {
            cout << "Finish cropping input Nrrd data" << endl;
        }

        // generate range based on cropped data
        NrrdRange* range_GFP = nrrdRangeNew(lspNan(0), lspNan(0));
        NrrdRange* range_RFP = nrrdRangeNew(lspNan(0), lspNan(0));
        airMopAdd(mop, range_GFP, (airMopper)nrrdRangeNix, airMopAlways);
        airMopAdd(mop, range_RFP, (airMopper)nrrdRangeNix, airMopAlways);

        // generate range
        generateRange(nin_cropped, range_GFP, range_RFP, opt.rangeMinPercentile, opt.rangeMaxPercentile, opt.verbose, mop);

        // *********************** alone z-axis ******************************
        makeProjImage(nin, "z", 0.0, 1.0, imageOutPath_z, range_GFP, range_RFP, opt.verbose, mop);
        // *********************** alone x-axis ******************************
        // left
        makeProjImage(nin, "x", 0.0, 0.5, imageOutPath_x_left, range_GFP, range_RFP, opt.verbose, mop);
        // right
        makeProjImage(nin, "x", 0.5, 1.0, imageOutPath_x_right, range_GFP, range_RFP, opt.verbose, mop);

        // stitch and save the image
        stitchImages(imageOutPath_x_left, imageOutPath_z, imageOutPath_x_right, common_prefix);

        auto stop = chrono::high_resolution_clock::now(); 
        auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 
        // non single file mode, print out this message
        if (!opt.isSingleFile)
        {
            cout << endl << "Processed " << nhdr_name << " took " << duration.count() << " seconds" << endl << endl; 
        }
    }

    // non single file mode, generate video
    if (!opt.isSingleFile)
    {
        Resamp::makeVideo();
    }

    return;
}

// generate videos, only when not in the single file mode
void Resamp::makeVideo()
{
    int numFiles = opt.numFiles;

    // get the size by reading the first frame image
    cv::Size s = cv::imread(opt.out_path + "/001_joined.png").size();
    
    string out_file;
    if (opt.maxFileNum != "")
        out_file = opt.out_path + "/result_3view_" + opt.maxFileNum + ".avi";
    else
        out_file = opt.out_path + "/result_3view.avi";

    if (opt.maxFileNum != "")
        cout << "===================== result_3view_" + opt.maxFileNum + ".avi =====================" << std::endl;
    else
        cout << "===================== result_3view.avi =====================" << std::endl;
        

    // write the images to video with opencv video writer
    // VideoWriter (const String &filename, int fourcc, double fps, Size frameSize, bool isColor=true)
    // If FFMPEG is enabled, using codec=0; fps=0; you can create an uncompressed (raw) video file. 
    cv::VideoWriter vw(out_file.c_str(), cv::VideoWriter::fourcc('F', 'F', 'V', '1'), opt.fps, s, true);
    
    if(!vw.isOpened()) 
        std::cout << "cannot open videoWriter." << std::endl;
    
    // determine the time stamps, if starting with 0, it is 0min, if starting with 1, it is 2min
    int timestamp_min = stoi(opt.allValidFiles[0].second) * 2;
    int timestamp_hour = 0;
    string curText;
    for(int i = 0; i < numFiles; i++)
    {
        string frameNum = opt.allValidFiles[i].second;
        std::string name = opt.out_path + "/" + frameNum + "_joined.png";
        cv::Mat curImage = cv::imread(name);
        // put white text indicating frame number on the bottom left cornor of images
        // void putText(Mat& img, const string& text, Point org, int fontFace, double fontScale, Scalar color, int thickness=1, int lineType=8, bool bottomLeftOrigin=false )
        if (timestamp_min >= 60)
        {
            timestamp_min = 0;
            timestamp_hour++;
            if (timestamp_hour < 10)
            {
                curText = "0" + to_string(timestamp_hour) + ":00";
            }
            else
            {
                curText = to_string(timestamp_hour) + ":00";
            }
            putText(curImage, curText, cv::Point2f(20, s.height-20), cv::FONT_HERSHEY_SIMPLEX, 1.5, cv::Scalar(255,255,255), 3, 2, false);
            vw << curImage;
        }
        else
        {
            timestamp_min = timestamp_min + 2;
            if (timestamp_hour < 10 && timestamp_min < 10)
            {
                curText = "0" + to_string(timestamp_hour) + ":" + "0" + to_string(timestamp_min);
            }
            else if (timestamp_hour < 10 && timestamp_min >= 10)
            {
                curText = "0" + to_string(timestamp_hour) + ":" + to_string(timestamp_min);
            }
            else if (timestamp_hour >= 10 && timestamp_min < 10)
            {
                curText = to_string(timestamp_hour) + ":" + "0" + to_string(timestamp_min);
            }
            else if (timestamp_hour >= 10 && timestamp_min >= 10)
            {
                curText = to_string(timestamp_hour) + ":" + to_string(timestamp_min);
            }
            putText(curImage, curText, cv::Point2f(20, s.height-20), cv::FONT_HERSHEY_SIMPLEX, 1.5, cv::Scalar(255,255,255), 3, 2, false);
            vw << curImage;
        }
    }
    vw.release();
}