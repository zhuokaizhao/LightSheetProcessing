//
// Created by Jake Stover on 5/9/18.
// Modifed by Zhuokai Zhao
//

#include <teem/nrrd.h>
#include "corrimg.h"
#include "util.h"
#include "skimczi.h"

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

#include <chrono> 

using namespace std;
namespace fs = boost::filesystem;

void setup_corrimg(CLI::App &app) 
{
    auto opt = std::make_shared<corrimgOptions>();
    auto sub = app.add_subcommand("corrimg", "Process NRRD projection files and generate corresponding images to be used for cross correlation.");

    sub->add_option("-i, --proj_path", opt->proj_path, "Input original NRRD projection files path")->required();
    sub->add_option("-o, --image_path", opt->image_path, "Ouput path which contains corresponding output images")->required();
    // input and output file names will be determined later
    //sub->add_option("-i, --input", opt->input_file, "Input projection nrrd.");
    //sub->add_option("-o, --output", opt->output_file, "Output file name.")->required();
    sub->add_option("-k, --kernel", opt->kernel, "Kernel to use in resampling. (Default: Gaussian:10,4)");
    sub->add_option("-v, --verbose", opt->verbose, "Print processing message or not. (Default: 0(close))");

    sub->set_callback([opt]() 
    {
        // first determine if input proj_path is valid
        if (checkIfDirectory(opt->proj_path))
        {
            cout << "proj input directory " << opt->proj_path << " is valid" << endl;
            
            // obtain all the files
            const vector<string> files = GetDirectoryFiles(opt->proj_path);
            
            // count the number of files, note that the number starts counting at 0
            int projNum = 0;

            // vector of pairs which stores each file's extracted serial number and its name 
            vector< pair<int, string> > allValidFiles;
            
            for (int i = 0; i < files.size(); i++) 
            {
                string curFile = files[i];
                // check if input file is a .nrrd file
                int proj_suff = curFile.rfind(".nrrd");
                if ( (proj_suff != string::npos) && (proj_suff == curFile.length() - 5))
                {
                    if (opt->verbose)
                        cout << "Current input file " + curFile + " ends with .nrrd, count this file" << endl;
                    
                    projNum++;
                
                    // now we need to understand the sequence number of this file
                    int start = -1;
                    int end = curFile.rfind("-proj");
                    
                    // current file name without type
                    string curFileName = curFile.substr(0, proj_suff);
                    
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
                        allValidFiles.push_back( make_pair(stoi(sequenceNumString), curFileName) );
                    }
                    else
                    {
                        cout << "WARNING: " << sequenceNumString << " is NOT a number" << endl;
                    }
                }

            }

            // after finding all the files, sort the allValidFiles in ascending order
            sort(allValidFiles.begin(), allValidFiles.end());

            cout << projNum/3 << " NRRD projection files (for each XY, XZ, YZ direction) found in input path " << opt->proj_path << endl << endl;

            // sanity check
            if (projNum != allValidFiles.size())
            {
                cout << "ERROR: Not all valid files have been recorded" << endl;
            }

            for (int i = 0; i < allValidFiles.size(); i++)
            {
                // get opt ready for corrimg
                opt->input_file = opt->proj_path + allValidFiles[i].second + ".nrrd";
                opt->output_file = opt->image_path + allValidFiles[i].second + ".png";

                // test if the output already exists
                // when output already exists, skip this iteration
                if (fs::exists(opt->output_file))
                {
                    cout << opt->output_file << " exists, continue to next." << endl;
                    continue;
                }

                try 
                {
                    cout << "Currently processing projection file " << opt->input_file << endl;
                    auto start = chrono::high_resolution_clock::now();
                    Corrimg(*opt).main();
                    auto stop = chrono::high_resolution_clock::now(); 
                    auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 
                    cout << "Output " << opt->output_file << " has been saved successfully" << endl;
                    cout << "Processing took " << duration.count() << " seconds" << endl << endl; 
                } 
                catch(LSPException &e) 
                {
                    std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
                }
            }
        }
        else
        {
            // the program also handles if input file is a single file
            cout << opt->proj_path << " is not a directory, check if it is a valid .nrrd file" << endl;
            const string curFile = opt->proj_path;

            std::cout << "Current file name is: " << curFile << endl;
            
            // check if input file is a .czi file
            int suff = curFile.rfind(".nrrd");
            int start = curFile.rfind("/");
            int length = suff - start - 1;

            if ( (suff != string::npos) || (suff != curFile.length() - 5)) 
            {
                cout << "Current input file " + curFile + " does not end with .nrrd, error" << endl;
                return;
            }
            else
            {
                cout << "Current input file " + curFile + " ends with .nrrd, process this file" << endl;  

                // test if the output already exists
                // when output already exists, skip this iteration
                if (fs::exists(opt->output_file))
                {
                    cout << opt->output_file << " exists, program ends." << endl;
                    return;
                }

                try
                {
                    opt->input_file = opt->proj_path;
                    opt->output_file = opt->image_path + curFile.substr(start+1, length) + ".png";
                    auto start = chrono::high_resolution_clock::now();
                    Corrimg(*opt).main();
                    auto stop = chrono::high_resolution_clock::now(); 
                    auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 
                    cout << "Output " << opt->output_file << " has been saved successfully" << endl;
                    cout << "Processing took " << duration.count() << " seconds" << endl << endl; 
                }
                catch(LSPException &e)
                {
                    std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
                }
            }
        }
    });
}


Corrimg::Corrimg(corrimgOptions const &opt): opt(opt), mop(airMopNew()) 
{
    // create folder if it does not exist
    if (!checkIfDirectory(opt.image_path))
    {
        boost::filesystem::create_directory(opt.image_path);
        cout << "Resampled projection output path " << opt.image_path << " does not exits, but has been created" << endl;
    }

    // load input file
    nrrd1 = safe_nrrd_load(mop, opt.input_file);

    // create an empty file
    nrrd2 = safe_nrrd_new(mop, (airMopper)nrrdNuke);

}

Corrimg::~Corrimg()
{
    airMopOkay(mop);
}


void Corrimg::main() 
{
    // get nrrd sliced and change axis order(for faster speed)
    const unsigned int axes_permute[3] = {2, 0, 1};
    
    nrrd_checker(nrrdSlice(nrrd2, nrrd1, 3, 1) ||
                 nrrdAxesPermute(nrrd2, nrrd2, axes_permute),
                 mop, "Error slicing axis:\n", "corrimg.cpp", "Corrimg::main");

    
    // augment vals in odd channel
    size_t n = nrrdElementNumber(nrrd2);
    
    for (size_t i = 1; i < n; i += 2) 
    {
            double val = 3.0 * nrrdDLookup[nrrd2->type](nrrd2->data, i);
            nrrdDInsert[nrrd2->type](nrrd2->data, i, val);
    }

    // project mean val of one axis
    nrrd_checker(nrrdProject(nrrd1, nrrd2, 0, nrrdMeasureMean, nrrd2->type),
                mop, "Error projecting nrrd:\n", "corrimg.cpp", "Corrimg::main");

    auto rsmc = nrrdResampleContextNew();
    airMopAdd(mop, rsmc, (airMopper)nrrdResampleContextNix, airMopAlways);

    // set up nrrd kernel
    NrrdKernelSpec *kernel_spec = nrrdKernelSpecNew();
    nrrd_checker(nrrdKernelParse(&(kernel_spec->kernel), kernel_spec->parm, opt.kernel.c_str()),
                mop, "Error parsing kernel:\n", "corrimg.cpp", "Corrimg::main");

    airMopAdd(mop, kernel_spec, (airMopper)nrrdKernelSpecNix, airMopAlways);

    // resample nrrd data
    nrrd_checker(nrrdResampleInputSet(rsmc, nrrd1) ||
                    nrrdResampleKernelSet(rsmc, 0, kernel_spec->kernel, kernel_spec->parm) ||
                    nrrdResampleSamplesSet(rsmc, 0, nrrd1->axis[0].size) ||
                    nrrdResampleRangeFullSet(rsmc, 0) ||
                    nrrdResampleBoundarySet(rsmc, nrrdBoundaryWeight) ||
                    nrrdResampleRenormalizeSet(rsmc, AIR_TRUE) ||
                    nrrdResampleKernelSet(rsmc, 1, kernel_spec->kernel, kernel_spec->parm) ||
                    nrrdResampleSamplesSet(rsmc, 1, nrrd1->axis[1].size) ||
                    nrrdResampleRangeFullSet(rsmc, 1) ||
                    nrrdResampleExecute(rsmc, nrrd2),
                mop, "Error resampling nrrd:\n", "corrimg.cpp", "Corrimg::main");


    // quantize vals from 32 to 16 bits
    nrrd_checker(nrrdQuantize(nrrd1, nrrd2, NULL, 16),
                mop, "Error quantizing nrrd:\n", "corrimg.cpp", "Corrimg::main");

    // save final results
    nrrd_checker(nrrdSave(opt.output_file.c_str(), nrrd1, NULL),
                mop, "Could not save file:\n", "corrimg.cpp", "Corrimg::main");

}
