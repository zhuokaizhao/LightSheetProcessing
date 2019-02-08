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

    sub->add_option("-i, --image_path", opt->image_path, "Path that includes input images")->required();
    sub->add_option("-t, --image_type", opt->image_type, "Type of the image, max or ave")->required();
    sub->add_option("-k, --kernel", opt->kernel_path, "Path that includes the kernel file")->required();
    sub->add_option("-o, --out_path", opt->out_path, "Path that includes all the output images")->required();
    sub->add_option("-n, --max_file_number", opt->maxFileNum, "The max number of files that we want to process");

    sub->set_callback([opt])()
    {
        // first determine if input image_path is valid
        if ( checkIfDirectory(opt->image_path) )
        {
            cout << "image input directory " << opt->image_path << " is valid" << endl;

            // count the number of files
            const vector<string> images = GetDirectoryFiles(opt->image_path);

            // the number starts at 0
            int imageNum = 0;

            for (int i = 0; i < images.size(); i++)
            {
                string curImage = images[i];

                // check if input image is a .png file
                int png_suff = curImage.rfind(".png");
                if ( (png_suff != string::npos) && (png_suff == curImage.length()-4) )
                {
                    if (opt->verbose)
                    {
                        cout << "Current input file " + curImage + " ends with .png, count this file" << endl;
                    }

                    imageNum++;

                    // now we need to understand the sequence number of this file
                    // the naming of input images are like xxx-type.png
                    int start = -1;
                    // suffix is either -max.png or -avg.png
                    string suffix = "-" + type + ".png";
                    int end = curImage.rfind(suffix);
                    
                    // current file name without -max.png or -avg.png
                    string curImageName = curFile.substr(0, end);
                    
                    // The sequenceNumString will have zero padding, like 001
                    for (int i = 0; i < end; i++)
                    {
                        // we get the first position that zero padding ends so that we know the real number
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
                        opt->allValidImages.push_back( make_pair(stoi(sequenceNumString), (curImageName+"-"+type)) );
                    }
                    else
                    {
                        cout << "WARNING: " << sequenceNumString << " is NOT a number" << endl;
                    }
                }
            }

            // after finding all the files, sort the allValidFiles in ascending order
            sort(opt->allValidFiles.begin(), opt->allValidFiles.end());

            cout << imageNum << " " << type << " .png files found in input path " << opt->image_path << endl << endl;

            // sanity check
            if (imageNum != opt->allValidImages.size())
            {
                cout << "ERROR: Not all valid files have been recorded" << endl;
            }

            // if the user restricts the number of files to process
            if (!opt->maxFileNum.empty())
            {
                imageNum = stoi(opt->maxFileNum);
            }

            opt->imageNum = imageNum;
            cout << "Total number of .png images that we are processing is: " << opt->imageNum << endl << endl;

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
            // the program also handles if input file is a single image
            cout << opt->image_path << " is not a directory, check if it is a valid .png file" << endl;
            const string curImage = opt->image_path;

            std::cout << "Current image name is: " << curFile << endl;
            
            // check if input file is a .png file
            int suff = curImage.rfind(".png");

            if ( (suff != string::npos) || (suff != curFile.length() - 4)) 
            {
                cout << "Current input image " + curImage + " does not end with .png, error" << endl;
                return;
            }
            else
            {
                cout << "Current input image " + curImage + " ends with .png, process this file" << endl;

                // update file number, -1 triggers single mode
                opt->imageNum = -1;    

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
    if (!checkIfDirectory(opt.out_path))
    {
        boost::filesystem::create_directory(opt.out_path);
        cout << "Image output path " << opt.out_path << " does not exits, but has been created" << endl;
    }
}

Resamp::~Resamp()
{
    airMopOkay(mop);
}

// ********************** some static helper functions *********************
static int isEven (uint x)
{
    if (x % 2 == 0)
    {
        return 1;
    }

    return 0;
}


void Resamp::ConvoEval(lspCtx *ctx, double xw, double yw) 
{
    // initialize output
    ctx->wpos[0] = xw;
    ctx->wpos[1] = yw;
    ctx->outside = 0;
    ctx->value = lspNan(0);

    // first convert wpos to ipos, where ipos are x1 and x2 as in FSV
    // MV3_MUL only takes 3-vector
    real ipos[3];
    real wpos[3] = {ctx->wpos[0], ctx->wpos[1], 1};

    // 3-vector ipos = 3x3 matrix WtoI * 3-vector wpos
    MV3_MUL(ipos, ctx->WtoI, wpos);

    // set this to ctx
    ctx->ipos[0] = ipos[0];
    ctx->ipos[1] = ipos[1];

    // determine different n1, n2 for even and odd kernels
    int n1, n2;
    // determine lower and upper bounds for later convolution
    int lower, upper;

    // even kernel
    if ( isEven(ctx->kern->support) )
    {
        // n1 = floor(x1), n2 = floor(x2)
        n1 = floor(ctx->ipos[0]);
        n2 = floor(ctx->ipos[1]);
        // lower = 1 - support/2
        lower = 1 - (int)ctx->kern->support / 2;
        // upper = support/2
        upper = (int)ctx->kern->support / 2;
    }
    // odd kernel
    else
    {
        // n1 = floor(x1+0.5), n2 = floor(x2+0.5)
        n1 = floor(ctx->ipos[0] + 0.5);
        n2 = floor(ctx->ipos[1] + 0.5);
        // lower = (1 - support)/2
        lower = (int)(1 - ctx->kern->support) / 2;
        // upper = (support - 1)/2
        upper = (int)(ctx->kern->support - 1) / 2;
    }

    // calculate alpha based on n1, n2
    real alpha1, alpha2;
    alpha1 = ctx->ipos[0] - n1;
    alpha2 = ctx->ipos[1] - n2;

    // separable convolution
    real sum = 0;
    real sum_d1 = 0, sum_d2 = 0;

    // initialize kernels
    real k1[ctx->kern->support], k2[ctx->kern->support];
    // kernel for derivatives (kern->deriv points back to itself when no gradient)
    real k1_d[ctx->kern->deriv->support], k2_d[ctx->kern->deriv->support];

    // precompute two vectors of kernel evaluations to save time
    for (int i = lower; i <= upper; i++)
    {
        k1[i - lower] = ctx->kern->eval(alpha1 - i);
        k2[i - lower] = ctx->kern->eval(alpha2 - i);

        // if gradient is true, kernel is calculated with gradient
        if (needgrad)
        {
            k1_d[i - lower] = ctx->kern->deriv->eval(alpha1 - i);
            k2_d[i - lower] = ctx->kern->deriv->eval(alpha2 - i);
        }
    }

    // compute via two nested loops over the 2D-kernel support
    // make sure image is not empty
    if (ctx->image != NULL)
    {
        // check for potential outside
        // Notice that both direction should be checked separately
        for (int i1 = lower; i1 <= upper; i1++)
        {
            if ( i1 + n1 < 0 || i1 + n1 >= (int)ctx->image->size[0] )
            {
                ctx->outside += 1;
            }
        }
        for (int i2 = lower; i2 <= upper; i2++)
        {
            if ( i2 + n2 < 0 || i2 + n2 >= (int)ctx->image->size[1] )
            {
                ctx->outside += 1;
            }
        }

        if (ctx->outside == 0)
        {
            // faster axis first (i1 fast)
            for (int i2 = lower; i2 <= upper; i2++)
            {
                for (int i1 = lower; i1 <= upper; i1++)
                {
                    // not outside
                    sum = sum + ctx->image->data.rl[ (n2+i2)*ctx->image->size[0] + n1 + i1 ] * k1[i1-lower] * k2[i2-lower];

                    if (needgrad)
                    {
                        sum_d1 = sum_d1 + ctx->image->data.rl[ (n2+i2)*ctx->image->size[0] + n1 + i1 ] * k1_d[i1-lower] * k2[i2-lower];
                        sum_d2 = sum_d2 + ctx->image->data.rl[ (n2+i2)*ctx->image->size[0] + n1 + i1 ] * k1[i1-lower] * k2_d[i2-lower];
                    }
                }
            }

            // if no outside, assign value
            ctx->value = sum;
            // derivative
            if (needgrad)
            {
                // convert from index-space to world space
                real gradient_w[2];
                real gradient_i[2] = {sum_d1, sum_d2};
                MV2_MUL(gradient_w, ctx->ItoW_d, gradient_i);

                // save the *world-space* gradient result in ctx->gradient
                V2_COPY(ctx->gradient, gradient_w);
            }
        }
    }
    // ^'^'^'^'^'^'^'^'^'^'^'^'^'^'^  end student code (83L in ref)

    return;
}


// main function
void Resamp::main()
{
    // the number of images
    int imageNum = opt.imageNum;

    // if imageNum == -1, single image mode
    if (imageNum == -1)
    {
        // load the image first
        lspImage *image = lspImageNew();
        airMopAdd(mop, image, (airMopper)lspImageNix, airMopAlways);
        int loadImageSuccess = lspImageLoad(image, opt.image_path);
        if (!loadImageSuccess)
        {
            cout << "Error loading image with path " << opt.image_path << endl;
            return;
        }

        // save the output image
        int saveImageSuccess = lspImageSave(opt.out_path, image);

        if (!saveImageSuccess)
        {
            cout << "Error saving image to path " << opt.out_path << endl;
        }

        // lspCtx* ctx = mprCtxNew(lspImg, mprKernelBox);
    }
}