//
// Created by Jake Stover on 5/9/18.
// Modified by Zhuokai Zhao
//

#include <boost/filesystem.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <corr.h>

#include "util.h"
#include "skimczi.h"

#include "corrfind.h"

void setup_corrfind(CLI::App &app) 
{
    auto opt = std::make_shared<corrfindOptions>();

    auto sub = app.add_subcommand("corrfind", "Computes the shift between images with sequence numbers i and i-1. Expects filenames to be similar to 001-{XY,XZ,YX}.png");

    sub->add_option("-i, --image_path", opt->image_path, "Input path that contains images where shifts are going to be found")->required();
    sub->add_option("-o, --align_path", opt->align_path, "Output path that will contain optimal alignment results")->required();
    //sub->add_option("-f, --file_number", opt->file_number, "File number. This command expects files to be of the form reg/%d-{XY,XZ,YX}.png.")->required();
    //sub->add_option("-o, --output", opt->output_name, "Base name to use when saving out the optimal alignemnt of images. (Default: -corr1.txt)");
    
    // optional arguments
    sub->add_option("-k, --kernels", opt->kernels, "Kernels to pass to lsp corr. (Default: c4hexic c4hexicd)")->expected(2);
    sub->add_option("-b, --bound", opt->bound, "Max offset to be passed to lsp corr. (Default: 10)");
    sub->add_option("-e, --epsilon", opt->epsilon, "Epsilon to be passed to lsp corr. (Default: 0.00000000000001)");
    sub->add_option("-v, --verbose", opt->verbose, "Print processing message or not. (Default: 0(close))");

    sub->set_callback([opt]() 
    {
        // if input is a file path
        if (checkIfDirectory(opt->image_path))
        {
            cout << "Input path " << opt->image_path << " is valid, start processing" << endl << endl;
            // count the number of images
            const vector<string> images = GetDirectoryFiles(opt->image_path);
            int numImages = 0;
            for (int i = 0; i < images.size(); i++)
            {
                // get the current file
                string curImage = images[i];
                
                // check if input file is a .png file
                int end = curImage.rfind(".png");
                
                // if this is indeed an image
                if ( (end != string::npos) && (end == curImage.length() - 4) )
                {
                    numImages++;
                }
            }

            // update this number with opt
            opt->file_number = numImages;

            try 
            {
                Corrfind(*opt).main();
            } 
            catch(LSPException &e) 
            {
                std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
            }
        }
    });
}


Corrfind::Corrfind(corrfindOptions const &opt): opt(opt), mop(airMopNew()) {}


Corrfind::~Corrfind()
{
    airMopOkay(mop);
}


void Corrfind::main() 
{
    //std::string output_name = opt.image_path + zero_pad(opt.file_number, 3) + opt.output_name;
    std::ofstream outfile(opt.output_name);

    // if only one file is passed
    if (opt.file_number == 0 || opt.file_number == 1) 
    {
        outfile << std::vector<double>{0, 0, 0, 0} << std::endl;
    } 
    else
    {
        // generate opt for corr
        corrOptions opt_corr;
        opt_corr.image_path = opt.image_path;
        opt_corr.output_path = opt.align_path;
        opt_corr.verbose = opt.verbose;
        opt_corr.kernel = opt.kernels;
        opt_corr.max_offset = opt.bound;
        opt_corr.epsilon = opt.epsilon;

        // output
        std::vector<double> shifts = corr_main(opt_corr);

        // we take the average of the top 2 xx/yy/zz as the final result
        double xx = (shifts[0] + shifts[2])/2.0;
        double yy = (shifts[1] + shifts[4])/2.0;
        double zz = (shifts[3] + shifts[5])/2.0;

        outfile << std::vector<double>{xx, yy, zz, AIR_CAST(double, opt.file_number)} << std::endl;
    }

    outfile.close();
}