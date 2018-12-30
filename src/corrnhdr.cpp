//
// Created by Jake Stover on 5/9/18.
// Modified by Zhuokai Zhao

#include <boost/filesystem.hpp>
#include <iostream>
#include <regex>

#include <teem/nrrd.h>

#include "skimczi.h"
#include "util.h"
#include "corrnhdr.h"

using namespace std;
namespace fs = boost::filesystem;

void setup_corrnhdr(CLI::App &app) 
{
    auto opt = std::make_shared<corrnhdrOptions>();
    auto sub = app.add_subcommand("corrnhdr", "Apply the corrections calculated by corrimg and corrfind.");

    sub->add_option("-n, --nhdr_path", opt->nhdr_path, "Input path for all the nhdr files")->required();
    sub->add_option("-c, --corr_path", opt->corr_path, "Input path for correlation results")->required();
    sub->add_option("-o, --new_nhdr_path", opt->new_nhdr_path, "Output ")->required();
    sub->add_option("-v, --verbose", opt->verbose, "Print processing message or not. (Default: 0(close))");

    sub->set_callback([opt] 
    {
        try
        {
            // read shifts and offsets from input file
            // check if input_path is valid, notice that there is no Single file mode for this task, has to be directory
            if (checkIfDirectory(opt->corr_path) && checkIfDirectory(opt->nhdr_path))
            {
                cout << "Input correlation path " << opt->corr_path << " is valid, start processing" << endl << endl;

                const vector<string> files = GetDirectoryFiles(opt->corr_path);

                // vector of pairs which stores each txt file's extracted serial number and its name
                vector< pair<int, string> > allValidFiles;

                // count the number of .txt files
                int numCorr = 0;

                for (int i = 0; i < files.size(); i++)
                {
                    // check if input file is a .txt file
                    string curFile = files[i];
                    int end = curFile.rfind(".txt");

                    // if this is indeed a valid .txt file
                    if ( (end != string::npos) && (end == curFile.length() - 4) )
                    {
                        numCorr++;

                        // now we need to understand the sequence number of this file
                        int start = -1;
                        
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
                            allValidFiles.push_back( make_pair(stoi(sequenceNumString), curFileName) );
                        }
                        else
                        {
                            cout << "WARNING: " << sequenceNumString << " is NOT a number" << endl;
                        }
                    }

                }

                // after finding all the files, sort the allFileSerialNumber in ascending order
                sort(allValidFiles.begin(), allValidFiles.end());

                cout << numCorr << " .txt correlation results found in input path " << opt->corr_path << endl << endl;

                // sanity check
                if (numCorr != allValidFiles.size())
                {
                    cout << "ERROR: Not all valid files have been recorded" << endl;
                }

                // organize information into opt
                opt->allValidFiles = allValidFiles;
                opt->num = numCorr;

                // run the corrnhdr main
                Corrnhdr(*opt).main();
            }
        } 
        catch (LSPException &e) 
        {
            std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
        }
    });
}


Corrnhdr::Corrnhdr(corrnhdrOptions const &opt): opt(opt), mop(airMopNew()), nhdr_path(opt.nhdr_path), corr_path(opt.corr_path), new_nhdr_path(opt.new_nhdr_path)
{
    // check if nhdr_path exists
    if(!fs::exists(nhdr_path))
    {
        throw LSPException("Error finding 'nhdr_path' subdirectory.", "corrnhdr.cpp", "Corrnhdr::Corrnhdr");
    }
        
    // check if corr_path exists
    if(!fs::exists(corr_path))
    {
        throw LSPException("Error finding 'corr_path' subdirectory.", "corrnhdr.cpp", "Corrnhdr::Corrnhdr");
    }

    // create new_nhdr_path if it does not exist
    if (!checkIfDirectory(opt.new_nhdr_path))
    {
        boost::filesystem::create_directory(opt.new_nhdr_path);
        cout << "Output path " << opt.new_nhdr_path << " does not exist, but has been created" << endl;
    }

    offset_origin = safe_nrrd_new(mop, (airMopper)nrrdNix);
    offset_median = safe_nrrd_new(mop, (airMopper)nrrdNuke);
    offset_smooth = safe_nrrd_new(mop, (airMopper)nrrdNuke);
}


Corrnhdr::~Corrnhdr()
{
    airMopOkay(mop);
}


void Corrnhdr::compute_offsets(int i)
{
    // current offset from previous frame
    vector<double> curShift;

    // current offset from first frame
    vector<double> curOffset; 

    // path for the current input TXT correlation result
    fs::path corrfile = opt.corr_path + opt.allValidFiles[i].second + ".txt";
    
    // doubel check
    if (fs::exists(corrfile)) 
    {
        std::ifstream inFile;
        inFile.open(corrfile.string());
        
        for (auto j: {0,1,2}) 
        {
            double x;
            inFile >> x;
            // generate curShift
            curShift.push_back(x);
            // curOffsets = offsets of previous + curShift
            // when we first start, we don't have anything stored, then we need to add 0
            if (opt.allOffsets.empty())
            {
                curOffset.push_back(0 + x);
            }
            // when not empty, we add
            else
            {
                curOffset.push_back(opt.allOffsets[opt.allOffsets.size()-1][j] + x);
            }
            
        }

        opt.allShifts.push_back(curShift);
        opt.allOffsets.push_back(curOffset);

        inFile.close();
    } 
    else 
    {
        cout << "[corrnhdr] WARN: Required input TXT correlation result " << corrfile.string() << " does not exist." << std::endl;
    }

}

void Corrnhdr::median_filtering()
{
    // slice nrrd by x axis and median yz shifts and join them back
    Nrrd *ntmp = safe_nrrd_new(mop, (airMopper)nrrdNuke);

    auto nsize = AIR_UINT(offset_origin->axis[0].size);
    auto mnout = AIR_CALLOC(nsize, Nrrd*);
    airMopAdd(mop, mnout, airFree, airMopAlways);

    for (int ni=0; ni<nsize; ni++) 
    {
        nrrd_checker(nrrdSlice(ntmp, offset_origin, 0, ni),
                    mop, "Error slicing nrrd:\n", "corrnhdr.cpp", "Corrnhdr::median_filtering");

        airMopAdd(mop, mnout[ni] = nrrdNew(), (airMopper)nrrdNuke, airMopAlways);

        nrrd_checker(nrrdCheapMedian(mnout[ni], ntmp, 1, 0, 2, 1.0, 256),
                    mop, "Error computing median:\n", "corrnhdr.cpp", "Corrnhdr::median_filtering");
        
    }
    
    nrrd_checker(nrrdJoin(offset_median, (const Nrrd*const*)mnout, nsize, 0, AIR_TRUE), 
                mop, "Error joining median slices:\n", "corrnhdr.cpp", "Corrnhdr::median_filtering");
    
    // copy axis info
    nrrdAxisInfoCopy(offset_median, offset_origin, NULL, NRRD_AXIS_INFO_NONE);
    nrrd_checker(nrrdBasicInfoCopy(offset_median, offset_origin,
                            NRRD_BASIC_INFO_DATA_BIT
                            | NRRD_BASIC_INFO_TYPE_BIT
                            | NRRD_BASIC_INFO_BLOCKSIZE_BIT
                            | NRRD_BASIC_INFO_DIMENSION_BIT
                            | NRRD_BASIC_INFO_CONTENT_BIT
                            | NRRD_BASIC_INFO_COMMENTS_BIT
                            | (nrrdStateKeyValuePairsPropagate
                            ? 0
                            : NRRD_BASIC_INFO_KEYVALUEPAIRS_BIT)),
                            mop, "Error copying nrrd info: ", "corrnhdr.cpp", "Corrnhdr::median_filtering");

}

// Used Gaussian filter to blur the image so that the impact of small features is reduced
void Corrnhdr::smooth()
{
    Nrrd *offset_blur = safe_nrrd_new(mop, (airMopper)nrrdNuke);

    auto rsmc1 = nrrdResampleContextNew();
    airMopAdd(mop, rsmc1, (airMopper)nrrdResampleContextNix, airMopAlways);

    //gaussian-blur
    double kparm[2] = {2, 3};
    nrrd_checker(nrrdResampleInputSet(rsmc1, offset_median) ||
                    nrrdResampleKernelSet(rsmc1, 0, NULL, NULL) ||
                    nrrdResampleBoundarySet(rsmc1, nrrdBoundaryBleed) ||
                    nrrdResampleRenormalizeSet(rsmc1, AIR_TRUE) ||
                    nrrdResampleKernelSet(rsmc1, 1, nrrdKernelGaussian, kparm) ||
                    nrrdResampleSamplesSet(rsmc1, 1, offset_median->axis[1].size) ||
                    nrrdResampleRangeFullSet(rsmc1, 1) ||
                    nrrdResampleExecute(rsmc1, offset_blur),
                    mop, "Error resampling median nrrd:\n", "corrnhdr.cpp", "Corrnhdr::smooth");
    
    // create helper nrrds array to help fix the boundary
    Nrrd *base = safe_nrrd_new(mop, (airMopper)nrrdNix);
    Nrrd *offset_bound = safe_nrrd_new(mop, (airMopper)nrrdNuke);

    //base = {1110000...0000111}
    std::vector<float> data(3, 1);
    data.insert(data.end(), 3*offset_blur->axis[1].size-6, 0);
    data.insert(data.end(), 3, 1);
    nrrd_checker(nrrdWrap_va(base, data.data(), nrrdTypeFloat, 2, 3, offset_blur->axis[1].size),
                mop, "Error wrapping data vector:\n", "corrnhdr.cpp", "Corrnhdr::smooth");
    nrrdAxisInfoCopy(base, offset_blur, NULL, NRRD_AXIS_INFO_ALL);

    //bound = gaussian(base)
    auto rsmc2 = nrrdResampleContextNew();
    airMopAdd(mop, rsmc2, (airMopper)nrrdResampleContextNix, airMopAlways);
    kparm[0] = 1.5;
    nrrd_checker(nrrdResampleInputSet(rsmc2, base) ||
                    nrrdResampleKernelSet(rsmc2, 0, NULL, NULL) ||
                    nrrdResampleBoundarySet(rsmc2, nrrdBoundaryBleed) ||
                    nrrdResampleRenormalizeSet(rsmc2, AIR_TRUE) ||
                    nrrdResampleKernelSet(rsmc2, 1, nrrdKernelGaussian, kparm) ||
                    nrrdResampleSamplesSet(rsmc2, 1, base->axis[1].size) ||
                    nrrdResampleRangeFullSet(rsmc2, 1) ||
                    nrrdResampleExecute(rsmc2, offset_bound),
                    mop, "Error resampling bound nrrd:\n", "corrnhdr.cpp", "Corrnhdr::smooth");

    //  nrrd_checker(nrrdQuantize(offset_bound2, offset_bound1, NULL, 32) ||
    //                nrrdUnquantize(offset_bound3, offset_bound2, nrrdTypeFloat),
    //              mop, "Error quantize/unquantize bound nrrd:\n", "corrnhdr.cpp", "Corrnhdr::smooth");

    // smooth the boundary
    NrrdIter *n1 = nrrdIterNew();
    NrrdIter *n2 = nrrdIterNew();
    NrrdIter *n3 = nrrdIterNew();

    airMopAdd(mop, n1, airFree, airMopAlways);
    airMopAdd(mop, n2, airFree, airMopAlways);
    airMopAdd(mop, n3, airFree, airMopAlways);

    nrrdIterSetOwnNrrd(n1, offset_bound);
    nrrdIterSetOwnNrrd(n2, offset_blur);
    nrrdIterSetOwnNrrd(n3, offset_origin);

    nrrdArithIterTernaryOp(offset_smooth, nrrdTernaryOpLerp, n1, n2, n3);

}


void Corrnhdr::main() 
{
    // change this to be loop for each input NHDR header and Corr result
    for (int i = 0; i < opt.num; i++)
    {
        // output file for the current loop
        fs::path outfile = opt.nhdr_path + GenerateOutName(i, 3, ".nhdr");
        cout << endl << "Currently generating new NHDR header named " << outfile << endl;
        
        // we want to check if current potential output file already exists, if so, skip
        if (fs::exists(outfile))
        {
            cout << outfile << " exists, continue to next." << endl << endl;
            continue;
        }

        // compute the offset with respect to the first frame of this current file
        vector<double> 
        compute_offsets(i);  

        median_filtering();
        smooth();

        //read spacing from each nhdr file
        double xs, ys, zs;
        std::ifstream ifile(opt.nhdr_path + GenerateOutName(i, 3, "nhdr"));
        std::string line;

        while(getline(ifile, line))
        {
            // we need to find directions
            if(line.find("directions:") != std::string::npos)
            {
                // patterns to be matched with the directions
                std::regex reg("\\((.*?), 0, 0\\).*\\(0, (.*?), 0\\).*\\(0, 0, (.*?)\\)");
                std::smatch res;

                if(std::regex_search(line, res, reg))
                {
                    xs = std::stof(res[1]);
                    ys = std::stof(res[2]);
                    zs = std::stof(res[3]);
                }
                break;
            }
        }
        ifile.close();

        //output files
        if (fs::exists(outfile)) 
        {
            //compute new origin
            double x_scale = nrrdDLookup[offset_smooth->type](offset_smooth->data, i*3);
            double y_scale = nrrdDLookup[offset_smooth->type](offset_smooth->data, i*3+1);
            double z_scale = nrrdDLookup[offset_smooth->type](offset_smooth->data, i*3+2);

            cout << "x_scale = " << x_scale << endl;
            cout << "y_scale = " << y_scale << endl;
            cout << "z_scale = " << z_scale << endl;

            std::string origin = "space origin: ("
                                + std::to_string(xs*x_scale) + ", "
                                + std::to_string(ys*y_scale) + ", "
                                + std::to_string(zs*z_scale) + ")";

            // std::string origin = "space origin: ("
            //                     + std::to_string(xs) + ", "
            //                     + std::to_string(ys) + ", "
            //                     + std::to_string(zs) + ")";

            cout << "Origin is " << origin << endl;

            //build new nhdr
            std::string o_name = opt.new_nhdr_path + GenerateOutName(i, 3, ".nhdr");
            std::ifstream ifile(outfile.string());
            std::ofstream ofile(o_name);

            std::string line;
            while(getline(ifile, line))
            {
                if(line.find("type:") != std::string::npos)
                {
                    // type should be the same, changed from signed short to ushort
                    ofile << "type: ushort" << std::endl;
                }
                else if(line.find("space origin:") != std::string::npos)
                {
                    ofile << origin << std::endl;
                }
                else
                {
                    ofile << line << std::endl;
                }
            }
        }
        else
        {   
            std::cout << "[corrnhdr] WARN: " << outfile.string() << " does not exist." << std::endl;
        }

    }

    // //change 2d vector to 2d array
    // double *data = AIR_CALLOC(3*opt.allOffsets.size(), double);
    // airMopAdd(mop, data, airFree, airMopAlways);
    
    // for(auto i = 0; i < 3*opt.allOffsets.size(); i++)
    // {
    //     data[i] = opt.allOffsets[i/3][i%3];
    // }
    

    // //save offsets into nrrd file
    // nrrd_checker(nrrdWrap_va(offset_origin, data, nrrdTypeDouble, 2, 3, opt.allOffsets.size()) ||
    //             nrrdSave((opt.corr_path+"offsets.nrrd").c_str(), offset_origin, NULL),
    //             mop, "Error creating offset nrrd:\n", "corrnhdr.cpp", "Corrnhdr::compute_offsets");
}
