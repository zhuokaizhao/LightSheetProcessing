// Modified by Zhuokai Zhao

#include <teem/nrrd.h>
#include "proj.h"
#include "util.h"
#include "skimczi.h"

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

// these are used while iterating filesystem
#include <dirent.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <memory>

#include <chrono> 

using namespace std;
namespace fs = boost::filesystem;

struct myclass {
  bool operator() (int i,int j) { return (i<j);}
} projSmallToLarge;

void setup_proj(CLI::App &app) 
{
    auto opt = std::make_shared<projOptions>();
    auto sub = app.add_subcommand("proj", "Create projection files based on nhdr file that has been generated by lsp skim.");

    sub->add_option("-i, --nhdr_path", opt->nhdr_path, "Input nhdr file path")->required();
    sub->add_option("-o, --proj_path", opt->proj_path, "Where to output projection files")->required();
    sub->add_option("-v, --verbose", opt->verbose, "Turn on (1) or off (0) debug messages, by default turned off");

    sub->set_callback([opt]() 
    {
        // vector of pairs which stores each nhdr file's name and its extracted serial number
        vector< pair<int, string> > allValidFiles;

        // first determine if input nhdr_path is valid
        if (checkIfDirectory(opt->nhdr_path))
        {
            cout << endl << "nhdr input directory " << opt->nhdr_path << " is valid" << endl;
            
            // count the number of files
            int nhdrNum = 0;
            // get all the files from input directory
            const vector<string> files = GetDirectoryFiles(opt->nhdr_path);

            // since files include .nhdr and .xml file in pairs, we want to count individual number
            for (const string curFile : files) 
            {
                // check if input file is a .nhdr file
                int nhdr_suff = curFile.rfind(".nhdr");
                if ( (nhdr_suff != string::npos) && (nhdr_suff == curFile.length() - 5))
                {
                    if (opt->verbose)
                        cout << "Current input file " + curFile + " ends with .nhdr, count this file" << endl;
                    
                    nhdrNum++;
                    // now we need to understand the sequence number of this file
                    int end = curFile.rfind(".nhdr");
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

            cout << nhdrNum << " .nhdr files found in input path " << opt->nhdr_path << endl << endl;

            // sanity check
            if (nhdrNum != allValidFiles.size())
            {
                cout << "ERROR: Not all valid files have been recorded" << endl;
            }

            // update file number
            opt->file_number = nhdrNum;
            cout << "Starting second loop for processing" << endl << endl;

            // another loop to process files
            for (int i = 0; i < allValidFiles.size(); i++)
            {   
                string proj_common = opt->proj_path + allValidFiles[i].second + "-proj";

                // we want to know if this proj file exists (processed before), don't overwrite it
                string proj_name_1 = proj_common + "XY.nrrd";
                string proj_name_2 = proj_common + "XZ.nrrd";
                string proj_name_3 = proj_common + "YZ.nrrd";
                fs::path projPath_1(proj_name_1);
                fs::path projPath_2(proj_name_2);
                fs::path projPath_3(proj_name_3);

                // when all three exists, skip this file
                if (fs::exists(projPath_1) && fs::exists(projPath_2) && fs::exists(proj_name_3))
                {
                    cout << "All " << proj_name_1 << ", " << proj_name_2 << ", " << proj_name_3 << " exist, continue to next." << endl;
                    opt->number_of_processed++;
                    cout << opt->number_of_processed << " out of " << opt->file_number << " files have been processed" << endl << endl;
                    continue;
                }

                // note that file name from allValidFiles does not include nhdr path
                opt->file_name = allValidFiles[i].second + ".nhdr";
                try
                {
                    auto start = chrono::high_resolution_clock::now();
                    Proj(*opt).main();
                    auto stop = chrono::high_resolution_clock::now(); 
                    auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 
                    opt->number_of_processed++;
                    cout << opt->number_of_processed << " out of " << opt->file_number << " files have been processed" << endl;
                    cout << "Processing " << opt->file_name << " took " << duration.count() << " seconds" << endl << endl; 
                }
                catch(LSPException &e)
                {
                    std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
                }
            }
        }
        // single file case
        else
        {
            // the program also handles if input file is a single file
            cout << opt->nhdr_path << " is not a directory, enter Single file mode" << endl;
            cout << "Checking if it is a valid .nhdr file" << endl;
            const string curFile = opt->nhdr_path;
            
            // check if input file is a .nhdr file
            int suff = curFile.rfind(".nhdr");

            if ( (suff == string::npos) || (suff != curFile.length() - 5) ) 
            {
                cout << "Current input file " + curFile + " does not end with .nhdr, error" << endl;
                return;
            }
            else
            {
                cout << "Current input file " + curFile + " ends with .nhdr, process this file" << endl;

                // update file number
                opt->file_number = 1;   
                opt->file_name = curFile;         
                try 
                {
                    auto start = chrono::high_resolution_clock::now();
                    Proj(*opt).main();
                    auto stop = chrono::high_resolution_clock::now(); 
                    auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 
                    cout << "Processing " << opt->file_name << " took " << duration.count() << " seconds" << endl; 
                } 
                catch(LSPException &e) 
                {
                    std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
                }
            }
        }
        
    });
}


Proj::Proj(projOptions const &opt): opt(opt), mop(airMopNew()) 
{
    if (!checkIfDirectory(opt.proj_path))
    {
        cout << opt.proj_path << " does not exist, but has been created" << endl;
        boost::filesystem::create_directory(opt.proj_path);
    }

    // if the number of file is 1 (single file since count starts at 1), use absolute name
    if (opt.file_number == 1)
    {
        nhdr_name = opt.nhdr_path;
        // in this case we keep the proj file name same as nhdr base name
        int end = nhdr_name.rfind(".nhdr");
        int start = nhdr_name.rfind("/");
        int length = end - start - 1;
        string fileName = nhdr_name.substr(start+1, length);
        if (opt.verbose)
            cout << "Nhdr name is " << fileName << endl;
        proj_common = opt.proj_path + fileName + "-proj";
    }
    else
    {
        nhdr_name = opt.nhdr_path + opt.file_name;

        cout << "Currently processing: " << nhdr_name << endl;

        // now we need to understand the sequence number of this file, which is the number after the baseName and before the extension
        string curFile = opt.file_name;
        int end = curFile.rfind(".nhdr");
        int start = 0;
        int length = end - start;
        string sequenceNumString = curFile.substr(start, length);
        proj_common = opt.proj_path + sequenceNumString + "-proj";
    }
}


Proj::~Proj(){
  airMopOkay(mop);
}


void Proj::main(){
    nrrdStateVerboseIO = 0;
    int verbose = opt.verbose;

    if (verbose)
        cout << "Start Proj::main()" << endl;

    // load input nhdr header file
    Nrrd* nin = safe_nrrd_load(mop, nhdr_name);

    //xy proj
    Nrrd* nproj_xy = safe_nrrd_new(mop, (airMopper)nrrdNuke);
    Nrrd* nproj_xy_t[2] = {safe_nrrd_new(mop, (airMopper)nrrdNuke),
                            safe_nrrd_new(mop, (airMopper)nrrdNuke)};
        
    std::string xy = proj_common + "XY.nrrd";

    nrrd_checker(nrrdProject(nproj_xy_t[0], nin, 3, nrrdMeasureMax, nrrdTypeFloat) ||
                    nrrdProject(nproj_xy_t[1], nin, 3, nrrdMeasureMean, nrrdTypeFloat) ||
                    nrrdJoin(nproj_xy, nproj_xy_t, 2, 3, 1), mop, "Error building XY projection:\n", "proj.cpp", "Proj::main");

    nrrdAxisInfoSet_va(nproj_xy, nrrdAxisInfoLabel, "x", "y", "c", "proj");
    // save
    nrrd_checker(nrrdSave(xy.c_str(), nproj_xy, nullptr),
                mop, "Error saving XY projection:\n", "proj.cpp", "Proj::main");
    

    cout << "X-Y Projection file has been saved to " << xy << endl;

    //xz proj
    unsigned int permute[4] = {0, 2, 1, 3}; //same permute array for xz and yz coincidently
    std::string xz = proj_common + "XZ.nrrd";
    Nrrd* nproj_xz = safe_nrrd_new(mop, (airMopper)nrrdNuke);
    Nrrd* nproj_xz_t[2] = {safe_nrrd_new(mop, (airMopper)nrrdNuke),
                            safe_nrrd_new(mop, (airMopper)nrrdNuke)};
    nrrd_checker(nrrdProject(nproj_xz_t[0], nin, 1, nrrdMeasureMax, nrrdTypeFloat) ||
                    nrrdProject(nproj_xz_t[1], nin, 1, nrrdMeasureMean, nrrdTypeFloat) ||
                    nrrdJoin(nproj_xz, nproj_xz_t, 2, 3, 1) ||
                    nrrdAxesPermute(nproj_xz, nproj_xz, permute), mop, "Error building XZ projection:\n", "proj.cpp", "Proj::main");

    nrrdAxisInfoSet_va(nproj_xz, nrrdAxisInfoLabel, "x", "z", "c", "proj");
    // save
    nrrd_checker(nrrdSave(xz.c_str(), nproj_xz, nullptr),
                mop, "Error saving XZ projection:\n", "proj.cpp", "Proj::main");

    cout << "X-Z Projection file has been saved to " << xz << endl;

    //yz proj
    Nrrd* nproj_yz = safe_nrrd_new(mop, (airMopper)nrrdNuke);
    Nrrd* nproj_yz_t[2] = {safe_nrrd_new(mop, (airMopper)nrrdNuke),
                            safe_nrrd_new(mop, (airMopper)nrrdNuke)};
    std::string yz = proj_common + "YZ.nrrd";
    nrrd_checker(nrrdProject(nproj_yz_t[0], nin, 0, nrrdMeasureMax, nrrdTypeFloat) ||
                    nrrdProject(nproj_yz_t[1], nin, 0, nrrdMeasureMean, nrrdTypeFloat) ||
                    nrrdJoin(nproj_yz, nproj_yz_t, 2, 3, 1) ||
                    nrrdAxesPermute(nproj_yz, nproj_yz, permute),
                mop, "Error building YZ projection:\n", "proj.cpp", "Proj::main");

    nrrdAxisInfoSet_va(nproj_yz, nrrdAxisInfoLabel, "y", "z", "c", "proj");

    // save
    nrrd_checker(nrrdSave(yz.c_str(), nproj_yz, nullptr),
                mop, "Error saving YZ projection:\n", "proj.cpp", "Proj::main");


    cout << "Y-Z Projection file has been saved to " << yz << endl;

}
