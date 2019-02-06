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

#include "anim.h"
#include "util.h"
#include "skimczi.h"
#include "resamp.h"

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
                string curImageName = images[i];

                // check if input image is a .png file
                int png_suff = curImageName.rfind(".png");
                if ( (png_suff != string::npos) && (png_) )
            }
        }
    }
}





void ConvoEval(mprCtx *ctx, real xw, real yw) 
{
    // figure out if you need to also measure the 1st derivative
    int needgrad = mprModeNeedsGradient(ctx->mode);
    // initialize output
    ctx->wpos[0] = xw;
    ctx->wpos[1] = yw;
    /* NOTE: you need to (in code below) set ctx->ipos to
       inverse of ctx->image->ItoW, multiplied by ctx->wpos */
    ctx->outside = 0;
    ctx->value = mprNan(0);
    if (needgrad) {
        ctx->gradient[0] = ctx->gradient[1] = mprNan(0);
    }

    // v.v.v.v.v.v.v.v.v.v.v.v.v.v.v  begin student code

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