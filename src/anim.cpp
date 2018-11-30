//
// Created by Jake Stover on 4/24/18.
// Modified by Zhuokai Zhao
//

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

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

using namespace std;
namespace fs = boost::filesystem;

struct myclass {
  bool operator() (int i,int j) { return (i<j);}
} animSmallToLarge;

void setup_anim(CLI::App &app) {
    auto opt = std::make_shared<animOptions>();
    auto sub = app.add_subcommand("anim", "Create animations from projection .nrrd files generated by skim.");

    sub->add_option("-i, --nhdr", opt->nhdr_path, "Path of input nrrd header files. (Default: ./nhdr/)")->required();
    sub->add_option("-p, --proj", opt->proj_path, "Path of input projection files. (Default: ./proj/)")->required();
    sub->add_option("-b, --base_name", opt->base_name, "The base name of nrrd files (For example: 181113)")->required();
    sub->add_option("-o, --anim", opt->anim_path, "Path of output anim files. (Default: ./anim/)")->required();
    sub->add_option("-n, --max_file_number", opt->maxFileNum, "The max number of files that we want to process");
    sub->add_option("-d, --dsample", opt->dwn_sample, "Amount by which to down-sample the data. (Default: 1.0)");
    sub->add_option("-x, --scalex", opt->scale_x, "Scaling on the x axis. (Default: 1.0)");
    sub->add_option("-z, --scalez", opt->scale_z, "Scaling on the z axis. (Default: 1.0)");
    sub->add_option("-v, --verbose", opt->verbose, "Print processing message or not. (Default: 0(close))");

    sub->set_callback([opt]() 
    { 
        // vector of strings that contains all the valid nhdr names
        vector<string> allNhdrFileNames;

        // first determine if input nhdr_path is valid
        if (checkIfDirectory(opt->nhdr_path))
        {
            cout << "nhdr input directory " << opt->nhdr_path << " is valid" << endl;
            
            // count the number of files
            const vector<string> files = GetDirectoryFiles(opt->nhdr_path);
            
            // note that the number starts counting at 1
            int nhdrNum = 0;

            // since files include .nhdr and .xml file in pairs, we want to count individual number
            for (int i = 0; i < files.size();; i++) 
            {
                // check if input file is a .nhdr file
                int nhdr_suff = curFile.rfind(".nhdr");
                if (nhdr_suff && (nhdr_suff == curFile.length() - 5))
                {
                    if (opt->verbose)
                        cout << "Current input file " + curFile + " ends with .nhdr, count this file" << endl;
                    nhdrNum++;
                    allNhdrFileNames.push_back(curFile);

                    // now we need to understand the sequence number of this file, which is the number after the baseName and before the extension
                    int end = curFile.rfind(".nhdr");
                    int start = curFile.rfind("_") + 1;
                    int length = end - start;
                    std::string sequenceNumString = curFile.substr(start, length);
                    
                    if (is_number(sequenceNumString))
                        opt->allFileSerialNumber.push_back(stoi(sequenceNumString));
                    else
                        cout << sequenceNumString << " is NOT a number" << endl;
                }

            }

            // after finding all the files, sort the allFileSerialNumber
            sort(opt->allFileSerialNumber.begin(), opt->allFileSerialNumber.end(), animSmallToLarge);

            cout << nhdrNum << " .nhdr files found in input path " << opt->nhdr_path << endl << endl;

            // vector size should equal to nhdrNum
            if (opt->allFileSerialNumber.size() != nhdrNum)
            {
                cout << "opt->allFileSerialNumber has size: " << opt->allFileSerialNumber.size() << endl;
                cout << "nhdrNum = " << to_string(nhdrNum) << endl;
                cout << "WARNING: allFileSerialNumber has wrong size" << endl;
            }

            // if the user restricts the number of files to process
            if (!opt->maxFileNum.empty())
            {
                nhdrNum = stoi(opt->maxFileNum);
            }
                
            // update file number
            opt->tmax = nhdrNum;
            cout << endl << "Total number of .nhdr files that we are processing is: " << opt->tmax << endl << endl;

            try
            {
                Anim(*opt).main();
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

            if (opt->verbose)
                std::cout << "Current file name is: " << curFile << endl;
            
            // check if input file is a .czi file
            int suff = curFile.rfind(".nhdr");

            if (!suff || (suff != curFile.length() - 5)) 
            {
                if (opt->verbose)
                    cout << "Current input file " + curFile + " does not end with .nhdr, error" << endl;
                return;
            }
            else
            {
                if (opt->verbose)
                    cout << "Current input file " + curFile + " ends with .nhdr, process this file" << endl;

                // update file number
                opt->tmax = 0;          
                try
                {
                    Anim(*opt).main();
                }
                catch(LSPException &e)
                {
                    std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
                }
            }
        }
        
    });
}


Anim::Anim(animOptions const &opt): opt(opt), mop(airMopNew()) 
{
    if (!checkIfDirectory(opt.anim_path))
    {
        boost::filesystem::create_directory(opt.anim_path);
        cout << "Anim output path " << opt.anim_path << " does not exits, but has been created" << endl;
    }
}

Anim::~Anim() {
    airMopOkay(mop);
}


int Anim::set_origins()
{
    // tmax count starts at 0, therefore the size allocated should be incremented by 1
    origins = std::vector<std::vector<int>>(opt.tmax+1, std::vector<int>(3, 0));

    //get origins for all projection files
    int found = 0;

    // distribute the work load of the for loop within the threads that have been created
    // tmax is the number of nrrd files, but its count starts at zero
    //#pragma omp parallel for
    for(int i = 0; i <= opt.tmax; i++)
    {
        //cout << "Current for loop with i = " << i << endl;
        // same zero padding as used when saving
        std::ifstream ifile;
        string nhdrFileName;

        nhdrFileName = opt.nhdr_path + opt.base_name + "_" + to_string(opt.allFileSerialNumber[i]) + ".nhdr";
        ifile.open(nhdrFileName);


        // when base_name is not empty, for example, it is 181113
        // base_name is required, just double check here
        /*
        if (!opt.base_name.empty())
        {
            // for example, nhdr_path is nhdr/, this would become nhdr/181113_0.nhdr
            nhdrFileName = opt.nhdr_path + opt.base_name + "_" + to_string(curNum) + ".nhdr";
            if (opt.verbose)
                cout << "Input nhdr file is: " << nhdrFileName << endl;
            ifile.open(nhdrFileName);
        }
        */
        // actually it is impossible, since base_name is now required
        /*
        else
        {
            nhdrFileName = opt.nhdr_path + zero_pad(i, 3) + ".nhdr";
            if (opt.verbose)
                cout << "Input nhdr file is: " << nhdrFileName << endl;
            ifile.open(nhdrFileName);
        }
        */

        std::string line;
        while(getline(ifile, line))
        {
            // finding space origin in .nhdr file
            if(line.find("space origin") != std::string::npos)
            {
                std::regex reg("\\((.*?), (.*?), (.*?)\\)");
                std::smatch res;
                if(std::regex_search(line, res, reg))
                {
                    // down-sample the data by input scale
                    origins[i][0] = std::stof(res[1])/opt.scale_x;
                    origins[i][1] = std::stof(res[2])/opt.scale_x;
                    origins[i][2] = std::stof(res[3])/opt.scale_z;
                    //#pragma omp atomic
                    found++;
                }
                cout << "Found space origin of " << nhdrFileName << endl;
                break;
            }
        }
        ifile.close();
    }

    //if all nhdr have origin field?
    if(found < opt.tmax+1)
    {
        cout << "Found " << found << " files instead of " << opt.tmax+1 << endl;
        //if(std::find(found.begin(), found.end(), 0)!=found.end()){
        std::cerr << "[ANIM WARNING]: (Part of) nhdr files lack of space origin field, will not implement origin relocation." << std::endl;
        return 1;
    }
    //if all origins are {0, 0, 0}.
    else if(!std::accumulate(origins.begin(), origins.end(), 0, [](int acc, std::vector<int> o){return acc || (o!=std::vector<int>(3, 0));}))
    {
        return 1;
    }

    //find minmax values in each dimension.
    for(auto i: {0, 1, 2})
    {
        int o_min = std::accumulate(origins.begin(), origins.end(), std::numeric_limits<int>::max(),
                                    [i](int acc, std::vector<int> o){return AIR_MIN(acc, o[i]);});
        
        int o_max = std::accumulate(origins.begin(), origins.end(), std::numeric_limits<int>::min(),
                                    [i](int acc, std::vector<int> o){return AIR_MAX(acc, o[i]);});
        
        minmax.push_back(std::vector<int>{o_min, o_max});
    }

    return 0;
}


void Anim::split_type()
{
    // set_origins reads from all nrrd files and extracts the origins
    int no_origin = set_origins();

    // why is resample_xy = scale_x/(scale_z * down_sample)
    // by default dwn_sample = 2, scale_x and scale_z are 1
    double resample_xy = opt.scale_x / opt.scale_z / opt.dwn_sample;
    double resample_z = 1.0 / opt.dwn_sample;

    if(opt.verbose)
        std::cout << "Resampling Factors: resample_xy = " + std::to_string(resample_xy) + ", resample_z = " + std::to_string(resample_z) << std::endl;

    // slice and resample projection files
    //#pragma omp parallel for
    for(int i = 0; i <= opt.tmax; i++) 
    {
        // mot_t is a new airArray
        auto mop_t = airMopNew();

        std::string iii = zero_pad(i, 3);

        if(opt.verbose)
        std::cout << "===== " + iii + "/" + std::to_string(opt.tmax) + " =====================\n";

        //read proj files
        string xy_proj_file, yz_proj_file;
        if (!opt.base_name.empty())
        {
            xy_proj_file = opt.proj_path + opt.base_name + "_" + to_string(opt.allFileSerialNumber[i]) + "-projXY.nrrd";
            yz_proj_file = opt.proj_path + opt.base_name + "_" + to_string(opt.allFileSerialNumber[i]) + "-projYZ.nrrd";
        }
        // this is technically impossible since base_name is required
        else
        {
            xy_proj_file = opt.proj_path + iii + "-projXY.nrrd";
            yz_proj_file = opt.proj_path + iii + "-projYZ.nrrd";
        }

        Nrrd* proj_rsm[2] = {safe_nrrd_load(mop_t, xy_proj_file),
                            safe_nrrd_load(mop_t, yz_proj_file)};

        //reset projs to space coordinate using new origins
        Nrrd* proj_t[2] = {safe_nrrd_new(mop_t, (airMopper)nrrdNuke),
                        safe_nrrd_new(mop_t, (airMopper)nrrdNuke)};
        
        // crop the area that we are going to perform resample
        if(!no_origin)
        {
            int x = origins[i][0], y = origins[i][1], z = origins[i][2];
            int minx = minmax[0][0], maxx = minmax[0][1];
            int miny = minmax[1][0], maxy = minmax[1][1];
            int minz = minmax[2][0], maxz = minmax[2][1];

            size_t min0[4] = {static_cast<size_t>(maxx-x), static_cast<size_t>(maxy-y), 0, 0};
            size_t max0[4] = {static_cast<size_t>(proj_rsm[0]->axis[0].size-x+minx)-1,
                                static_cast<size_t>(proj_rsm[0]->axis[1].size-y+miny)-1,
                                proj_rsm[0]->axis[2].size-1,
                                proj_rsm[0]->axis[3].size-1}; 
            size_t min1[4] = {static_cast<size_t>(maxy-y), static_cast<size_t>(maxz-z), 0, 0};
            size_t max1[4] = {static_cast<size_t>(proj_rsm[1]->axis[0].size-y+miny)-1,
                                static_cast<size_t>(proj_rsm[1]->axis[1].size-z+minz)-1,
                                proj_rsm[1]->axis[2].size-1,
                                proj_rsm[1]->axis[3].size-1};

            nrrd_checker(nrrdCrop(proj_t[0], proj_rsm[0], min0, max0) ||
                            nrrdCrop(proj_t[1], proj_rsm[1], min1, max1),
                            mop_t, "Error cropping nrrd:\n", "anim.cpp", "Anim::split_type");

            proj_rsm[0] = proj_t[0];
            proj_rsm[1] = proj_t[1];
        }

        //resample
        // initialize some new spaces
        Nrrd* res_rsm[2][2] = { //store {{max_z, avg_z}, {max_x, avg_x}}
                                {safe_nrrd_new(mop_t, (airMopper)nrrdNuke),
                                    safe_nrrd_new(mop_t, (airMopper)nrrdNuke)},
                                {safe_nrrd_new(mop_t, (airMopper)nrrdNuke),
                                    safe_nrrd_new(mop_t, (airMopper)nrrdNuke)}
                                };
        
        double resample_rsm[2][2] = {{resample_xy, resample_xy},
                                    {resample_xy, resample_z}};
        
        // k parameter
        double kparm[3] = {1, 0, 0.5};
        for(auto i=0; i<2; ++i){
        auto rsmc = nrrdResampleContextNew();
        airMopAdd(mop_t, rsmc, (airMopper)nrrdResampleContextNix, airMopAlways);

        nrrd_checker(nrrdResampleInputSet(rsmc, proj_rsm[i]) ||
                        nrrdResampleKernelSet(rsmc, 0, nrrdKernelBCCubic, kparm) ||
                        nrrdResampleSamplesSet(rsmc, 0, size_t(ceil(proj_rsm[i]->axis[0].size*resample_rsm[i][0]))) ||
                        nrrdResampleRangeFullSet(rsmc, 0) ||
                        nrrdResampleKernelSet(rsmc, 1, nrrdKernelBCCubic, kparm) ||
                        nrrdResampleSamplesSet(rsmc, 1, size_t(ceil(proj_rsm[i]->axis[1].size*resample_rsm[i][1]))) ||
                        nrrdResampleRangeFullSet(rsmc, 1) ||
                        nrrdResampleKernelSet(rsmc, 2, NULL, NULL) ||
                        nrrdResampleKernelSet(rsmc, 3, NULL, NULL) ||
                        nrrdResampleBoundarySet(rsmc, nrrdBoundaryBleed) ||
                        nrrdResampleRenormalizeSet(rsmc, AIR_TRUE) ||
                        nrrdResampleExecute(rsmc, proj_rsm[i]),
                        mop_t, "Error resampling nrrd:\n", "anim.cpp", "Anim::split_type");

            //SWAP(AX0 AX1) for yz plane
            if(i==1)
            {
                nrrd_checker(nrrdAxesSwap(proj_rsm[i], proj_rsm[i], 0, 1),
                            mop_t, "Error swaping yz axes:\n", "anim.cpp", "Anim::split_type");
            }

            nrrd_checker(nrrdSlice(res_rsm[i][0], proj_rsm[i], 3, 0) || 
                            nrrdSlice(res_rsm[i][1], proj_rsm[i], 3, 1),
                            mop_t, "Error slicing nrrd:\n", "anim.cpp", "Anim::split_type");

            airMopSingleOkay(mop_t, rsmc);
        }

        std::string max_z = opt.anim_path + zero_pad(i, 3) + "-max-z.nrrd";
        std::string max_x = opt.anim_path + zero_pad(i, 3) + "-max-x.nrrd";
        std::string avg_z = opt.anim_path + zero_pad(i, 3) + "-avg-z.nrrd";
        std::string avg_x = opt.anim_path + zero_pad(i, 3) + "-avg-x.nrrd";

        // save the resampled nrrd files
        nrrd_checker(nrrdSave(max_z.c_str(), res_rsm[0][0], nullptr) ||
                    nrrdSave(avg_z.c_str(), res_rsm[0][1], nullptr) ||
                    nrrdSave(max_x.c_str(), res_rsm[1][0], nullptr) ||
                    nrrdSave(avg_x.c_str(), res_rsm[1][1], nullptr),
                    mop_t, "Error saving nrrd:\n", "anim.cpp", "Anim::split_type");

        airMopOkay(mop_t);
    }
}


void Anim::make_max_frame(std::string direction){
  #pragma omp parallel for
  for(auto i=0; i<=opt.tmax; ++i){
    std::string common_prefix = opt.anim_path + zero_pad(i, 3) + "-max-" + direction;

    auto mop_t = airMopNew();

    Nrrd* ch0 = safe_nrrd_new(mop_t, (airMopper)nrrdNuke);
    Nrrd* ch1 = safe_nrrd_new(mop_t, (airMopper)nrrdNuke);
    Nrrd* bit0 = safe_nrrd_new(mop_t, (airMopper)nrrdNuke);
    Nrrd* bit1 = safe_nrrd_new(mop_t, (airMopper)nrrdNuke);

    //load iii-type-dir.nrrd files
    Nrrd* nin = safe_nrrd_load(mop_t, common_prefix + ".nrrd");

    //slice on channel
    nrrd_checker(nrrdSlice(ch0, nin, 2, 0) ||
                  nrrdSlice(ch1, nin, 2, 1),
                mop_t, "Error slicing nrrd:\n", "anim.cpp", "Anim::make_max_frame");

    //quantize to 8bit
    auto range0 = nrrdRangeNew(AIR_NAN, AIR_NAN);
    airMopAdd(mop_t, range0, (airMopper)nrrdRangeNix, airMopAlways);
    nrrd_checker(nrrdRangePercentileFromStringSet(range0, ch0,  "5%", "0.02%", 5000, true) ||
                  nrrdQuantize(bit0, ch0, range0, 8),
                mop_t, "Error quantizing ch1 nrrd:\n", "anim.cpp", "Anim::make_max_frame");

    //set brightness for ch1(and quantize to 8bit)
    auto range1 = nrrdRangeNew(AIR_NAN, AIR_NAN);
    airMopAdd(mop_t, range1, (airMopper)nrrdRangeNix, airMopAlways);
    nrrd_checker(nrrdArithGamma(ch1, ch1, NULL, 10) ||
                    nrrdRangePercentileFromStringSet(range1, ch1, "5%", "0.01%", 5000, true) ||
                    nrrdQuantize(bit1, ch1, range1, 8),
                mop_t, "Error quantizing ch2 nrrd:\n", "anim.cpp", "Anim::make_max_frame");

    if(opt.verbose)
      std::cout << "===== " + zero_pad(i, 3) + "/" + std::to_string(opt.tmax) + " " + direction + "_max_frames =====================\n";

    nrrd_checker(nrrdSave((common_prefix + "-0.ppm").c_str() , bit0, nullptr) ||
                  nrrdSave((common_prefix + "-1.ppm").c_str() , bit1, nullptr),
                mop_t, "Error saving ppm files:\n", "anim.cpp", "Anim::make_max_frame");

    airMopOkay(mop_t);
  }
}


void Anim::make_avg_frame(std::string direction){
  #pragma omp parallel for
  for(auto i=0; i<=opt.tmax; ++i){
    std::string common_prefix = opt.anim_path + zero_pad(i, 3) + "-avg-" + direction;

    auto mop_t = airMopNew();

    //load iii-type-dir.nrrd files
    Nrrd* nin = safe_nrrd_load(mop_t, common_prefix + ".nrrd");

    Nrrd* ch = safe_nrrd_new(mop, (airMopper)nrrdNuke);

    //resample: gaussian blur
    auto rsmc = nrrdResampleContextNew();
    airMopAdd(mop_t, rsmc, (airMopper)nrrdResampleContextNix, airMopAlways);

    double kparm[2] = {40,3};
    nrrd_checker(nrrdResampleInputSet(rsmc, nin) ||
                    nrrdResampleKernelSet(rsmc, 0, nrrdKernelGaussian, kparm) ||
                    nrrdResampleSamplesSet(rsmc, 0, nin->axis[0].size) ||
                    nrrdResampleRangeFullSet(rsmc, 0) ||
                    nrrdResampleBoundarySet(rsmc, nrrdBoundaryBleed) ||
                    nrrdResampleRenormalizeSet(rsmc, AIR_TRUE) ||
                    nrrdResampleKernelSet(rsmc, 1, nrrdKernelGaussian, kparm) ||
                    nrrdResampleSamplesSet(rsmc, 1, nin->axis[1].size) ||
                    nrrdResampleRangeFullSet(rsmc, 1) ||
                    nrrdResampleKernelSet(rsmc, 2, NULL, NULL) ||
                    nrrdResampleExecute(rsmc, ch),
                mop_t,  "Error resampling nrrd:\n", "anim.cpp", "Anim::make_avg_frame");

    //slice on ch 
    Nrrd* ch0 = safe_nrrd_new(mop_t, (airMopper)nrrdNuke);
    Nrrd* ch1 = safe_nrrd_new(mop_t, (airMopper)nrrdNuke);

    NrrdIter* nit1 = nrrdIterNew();
    NrrdIter* nit2 = nrrdIterNew();
    NrrdIter* nit3 = nrrdIterNew();
    NrrdIter* nit4 = nrrdIterNew();
    
    /* shared_ptr would not work
    std::shared_ptr<NrrdIter> nit1 (nrrdIterNew());
    std::shared_ptr<NrrdIter> nit2 (nrrdIterNew());
    std::shared_ptr<NrrdIter> nit3 (nrrdIterNew());
    std::shared_ptr<NrrdIter> nit4 (nrrdIterNew());
    */

    nrrdIterSetOwnNrrd(nit1, ch);
    nrrdIterSetValue(nit2, 0.5);
    nrrd_checker(nrrdArithIterBinaryOp(ch, nrrdBinaryOpMultiply, nit1, nit2),
                mop_t,  "Error doing BinaryMultiply nrrd:\n", "anim.cpp", "Anim::make_avg_frame");

    nrrdIterSetOwnNrrd(nit3, ch);
    nrrdIterSetOwnNrrd(nit4, nin);

    nrrd_checker(nrrdArithIterBinaryOp(nin, nrrdBinaryOpSubtract, nit4, nit3),
                mop_t,  "Error doing BinarySubstract nrrd:\n", "anim.cpp", "Anim::make_avg_frame");

    nrrd_checker(nrrdSlice(ch0, nin, 2, 0) ||
                  nrrdSlice(ch1, nin, 2, 1),
                mop_t,  "Error slicing nrrd:\n", "anim.cpp", "Anim::make_avg_frame");


    //quantize to 8bit
    Nrrd* bit0 = safe_nrrd_new(mop, (airMopper)nrrdNuke);
    Nrrd* bit1 = safe_nrrd_new(mop, (airMopper)nrrdNuke);
    auto range = nrrdRangeNew(AIR_NAN, AIR_NAN);
    airMopAdd(mop_t, range, (airMopper)nrrdRangeNix, airMopAlways);
    nrrd_checker(nrrdRangePercentileFromStringSet(range, ch0, "10%", "0.1%", 5000, true) ||
                  nrrdQuantize(bit0, ch0, range, 8) ||
                  nrrdRangePercentileFromStringSet(range, ch1, "10%", "0.1%", 5000, true) ||
                  nrrdQuantize(bit1, ch1, range, 8),
                mop_t, "Error quantizing nrrd:\n", "anim.cpp", "Anim::make_avg_frame");

    if(opt.verbose)
      std::cout << "===== " + zero_pad(i, 3) + "/" + std::to_string(opt.tmax) + " " + direction + "_avg_frames =====================\n";

    nrrd_checker(nrrdSave((common_prefix + "-0.ppm").c_str() , bit0, nullptr) ||
                  nrrdSave((common_prefix + "-1.ppm").c_str() , bit1, nullptr),
                mop_t, "Error saving ppm files:\n", "anim.cpp", "Anim::make_avg_frame");

    //TODO: In fact, we' better add cleanup function to mop, but nrrd lib only provide `nrrdIterNix` which will clean iter->nrrd also.
    //This simple free() here may cause memory leak.
    free(nit1);
    free(nit2);
    free(nit3);
    free(nit4);
    

    airMopOkay(mop_t);
  }
}

// build pngs for the animation
void Anim::build_png() 
{
    for(auto type: {"max", "avg"})
    {
        #pragma omp parallel for
        for(auto i=0; i<=opt.tmax; ++i)
        {
            auto mop_t = airMopNew();

            if(opt.verbose)
                std::cout << "===== " + zero_pad(i, 3) + "/" + std::to_string(opt.tmax) + " " + type + "_pngs =====================\n";

            std::string base_path = opt.anim_path + zero_pad(i, 3) + "-" + type;
            Nrrd *ppm_z_0 = safe_nrrd_load(mop_t, base_path + "-z-0.ppm");
            Nrrd *ppm_z_1 = safe_nrrd_load(mop_t, base_path + "-z-1.ppm");
            Nrrd *ppm_x_0 = safe_nrrd_load(mop_t, base_path + "-x-0.ppm");
            Nrrd *ppm_x_1 = safe_nrrd_load(mop_t, base_path + "-x-1.ppm");
            std::vector<Nrrd*> ppms_z = {ppm_z_1, ppm_z_0, ppm_z_1};
            std::vector<Nrrd*> ppms_x = {ppm_x_1, ppm_x_0, ppm_x_1};

            Nrrd *nout_z = safe_nrrd_new(mop_t, (airMopper)nrrdNuke);
            Nrrd *nout_x = safe_nrrd_new(mop_t, (airMopper)nrrdNuke);
            Nrrd *tmp_nout_array[2] = {nout_z, nout_x};
            Nrrd *nout = safe_nrrd_new(mop_t, (airMopper)nrrdNuke);

            nrrd_checker(nrrdJoin(nout_z, ppms_z.data(), ppms_z.size(), 0, 1) ||
                            nrrdJoin(nout_x, ppms_x.data(), ppms_x.size(), 0, 1) ||
                            nrrdJoin(nout, tmp_nout_array, 2, 1, 0),
                            mop_t, "Error joining ppm files to png:\n", "anim.cpp", "Anim::build_png");

            std::string out_name = base_path + ".png";
            nrrd_checker(nrrdSave(out_name.c_str(), nout, nullptr), 
                        mop_t, "Error saving png file:\n", "anim.cpp", "Anim::build_png");

            airMopOkay(mop_t);
        }
    }
}


// generate videos
void Anim::build_video(){
  int tmax = opt.tmax;
  std::string base_name = opt.anim_path;

  cv::Size s = cv::imread(base_name + "000-max.png").size();
  
  for(std::string type: {"max", "avg"}){ 
    if(opt.verbose)
      std::cout << "===== " + type + "_mp4 =====================" << std::endl;

    std::string out_file = base_name + type + ".avi";
    cv::VideoWriter vw(out_file.c_str(), cv::VideoWriter::fourcc('M','J','P','G'), 25, s, true);
    if(!vw.isOpened()) 
      std::cout << "cannot open videoWriter." << std::endl;
    for(auto i=0; i<=tmax; ++i){
      std::string name = base_name + zero_pad(i, 3) + "-" + type + ".png";
      vw << cv::imread(name);
    }
    vw.release();
  }
}


void Anim::main()
{
    int verbose = opt.verbose;
    if (verbose)
        cout << endl << "Anim::main() starts" << endl << endl;

    cout << "Splitting nrrd on type dimension" << endl;
    split_type();

    cout << "Making frames for max channel" << endl;
    make_max_frame("x");
    make_max_frame("z");

    cout << "Making frames for average channel" << endl;
    make_avg_frame("x");
    make_avg_frame("z");

    cout << "Building PNGs" << endl;
    build_png();

    cout << "Building video" << endl;
    build_video();
}
