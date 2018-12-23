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
using namespace boost::filesystem;

void setup_corrnhdr(CLI::App &app) 
{
    auto opt = std::make_shared<corrnhdrOptions>();
    auto sub = app.add_subcommand("corrnhdr", "Apply the corrections calculated by corrimg and corrfind.");

    //sub->add_option("-d, --file_dir", opt->file_dir, "Where 'nhdr/', nhdr-corr/' and 'reg/' are. Defualt path is working path. (Default: .)");
    sub->add_option("-n, --nhdr_path", opt->nhdr_path, "Input path for all the nhdr files")->required();
    // corr_path is the old reg
    sub->add_option("-c, --corr_path", opt->corr_path, "Input path for correlation results")->required();
    sub->add_option("-o, --new_nhdr_path", opt->new_nhdr_path, "Output ")->required();
    //sub->add_option("-n, --num_nhdr", opt->num, "The number of the last nhdr file.");

    sub->set_callback([opt] 
    {
        try
        {
            Corrnhdr(*opt).main();
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
    if(!exists(nhdr_path))
    {
        throw LSPException("Error finding 'nhdr_path' subdirectory.", "corrnhdr.cpp", "Corrnhdr::Corrnhdr");
    }
        
    // check if corr_path exists
    if(!exists(corr_path))
    {
        throw LSPException("Error finding 'corr_path' subdirectory.", "corrnhdr.cpp", "Corrnhdr::Corrnhdr");
    }

    // create new_nhdr_path if it does not exist
    if (!checkIfDirectory(opt.new_nhdr_path))
    {
        boost::filesystem::create_directory(opt.new_nhdr_path);
        cout << "Output path " << opt.new_nhdr_path << " does not exits, but has been created" << endl;
    }

    offset_origin = safe_nrrd_new(mop, (airMopper)nrrdNix);
    offset_median = safe_nrrd_new(mop, (airMopper)nrrdNuke);
    offset_smooth = safe_nrrd_new(mop, (airMopper)nrrdNuke);
}


Corrnhdr::~Corrnhdr()
{
    airMopOkay(mop);
}


void Corrnhdr::compute_offsets()
{
    std::vector<std::vector<double>> shifts,  //offset from previous frame
                                    offsets; //offset from first frame

    offsets.push_back({0, 0, 0});

    // read shifts and offsets from input file
    
    for (auto i = 0; i <= opt.num; i++) 
    {
        path file = file_dir + "/reg/" + zero_pad(i, 3) + "-corr1.txt";
        
        if (exists(file)) 
        {
            std::ifstream inFile;
            inFile.open(file.string());

            std::vector<double> tmp, tmp2;
            
            for (auto j: {0,1,2}) 
            {
                double x;
                inFile >> x;
                tmp.push_back(x);
                tmp2.push_back(offsets[offsets.size()-1][j] + x);
            }

            shifts.push_back(tmp);
            offsets.push_back(tmp2);

            inFile.close();
        } 
        else 
        {
            cout << "[corrnhdr] WARN: " << file.string() << " does not exist." << std::endl;
        }
    }
    offsets.erase(offsets.begin());   //Remove first entry of {0,0,0}

    //change 2d vector to 2d array
    double *data = AIR_CALLOC(3*offsets.size(), double);
    airMopAdd(mop, data, airFree, airMopAlways);
    for(auto i=0; i<3*offsets.size(); ++i)
    {
        data[i] = offsets[i/3][i%3];
    }
        

    //save offsets into nrrd file
    nrrd_checker(nrrdWrap_va(offset_origin, data, nrrdTypeDouble, 2, 3, offsets.size()) ||
                    nrrdSave((file_dir+"/reg/offsets.nrrd").c_str(), offset_origin, NULL),
                mop, "Error creating offset nrrd:\n", "corrnhdr.cpp", "Corrnhdr::compute_offsets");
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
    compute_offsets();
    median_filtering();
    smooth();

    //read spacing from first nhdr file
    double xs, ys, zs;
    std::ifstream ifile(file_dir+"/nhdr/000.nhdr");
    std::string line;
    while(getline(ifile, line))
    {
        if(line.find("directions:") != std::string::npos)
        {
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
  for (size_t i = 0; i <= opt.num; i++) 
  {
        path file = file_dir + "/nhdr/" + zero_pad(i, 3) + ".nhdr";
        if (exists(file)) 
        {
            //compute new origin
            double x_scale = nrrdDLookup[offset_smooth->type](offset_smooth->data, i*3),
                    y_scale = nrrdDLookup[offset_smooth->type](offset_smooth->data, i*3+1),
                    z_scale = nrrdDLookup[offset_smooth->type](offset_smooth->data, i*3+2);

            std::string origin = "space origin: ("
                                + std::to_string(xs*x_scale) + ", "
                                + std::to_string(ys*y_scale) + ", "
                                + std::to_string(zs*z_scale) + ")";

            //build new nhdr
            std::string o_name = file_dir + "/nhdr-corr/" + zero_pad(i, 3) + ".nhdr";
            std::ifstream ifile(file.string());
            std::ofstream ofile(o_name);

            std::string line;
            while(getline(ifile, line))
            {
                if(line.find("type:") != std::string::npos)
                ofile << "type: signed short" << std::endl;
                else if(line.find("space origin:") != std::string::npos)
                ofile << origin << std::endl;
                else
                ofile << line << std::endl;
            }
        }
        else
        {   
            std::cout << "[corrnhdr] WARN: " << file.string() << " does not exist." << std::endl;
        }
    }
}
