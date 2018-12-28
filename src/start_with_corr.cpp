// The program runs skim, proj, corrimg, corrfind and anim in LSP
// Created by Zhuokai Zhao
// Contact: zhuokai@uchicago.edu

#include <teem/nrrd.h>
#include "start_with_corr.h"
#include <chrono> 
#include "util.h"

// skim
#include <cstdio>
#include <cstdlib>
#include <cinttypes>
#include <sys/types.h>
#include <cfloat>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <cerrno>
#include <libxml/parser.h>
#include <cassert>

#include <teem/air.h>
#include <teem/biff.h>
#include <teem/ell.h>

#include <fstream>

#include "CLI11.hpp"

#include "skimczi.h"
#include "util.h"
#include "skimczi_util.h"

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

// these are used while iterating filesystem
#include <dirent.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <memory>

#include <chrono> 

// proj
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

// anim
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

#include <chrono>

#include "corrimg.h"
#include "corrfind.h"
#include "corrnhdr.h"

using namespace std;

namespace fs = boost::filesystem;

void start_standard_process_with_corr(CLI::App &app) 
{
    auto opt = make_shared<startwithcorrOptions>();
    auto sub = app.add_subcommand("startwithcorr", "Process that extends standard process with corrrelation computations to prevent drifts. Limited options though.");

    // czi path
    sub->add_option("-c, --czi_path", opt->czi_path, "Path for all the czi files")->required();
    // nhdr path
    sub->add_option("-n, --nhdr_path", opt->nhdr_path, "Path for nhdr header files")->required();
    // proj path
    sub->add_option("-p, --proj_path", opt->proj_path, "Path for projection files")->required();
    // image path from each proj file
    sub->add_option("-m, image_path", opt->image_path, "Path for all the images generated from each projection")->required();
    // correlation alignments results
    sub->add_option("-r, align_path", opt->align_path, "Path for all the TXT correlation results")->required();
    // new NHDR path
    sub->add_option("-h, new_nhdr_path", opt->new_nhdr_path, "Path for all the new NHDR headers")->required();
    // new projection path
    sub->add_option("-j, new_proj_path", opt->new_proj_path, "Path for all the new NRRD projection files from new headers")->required();
    // anim path
    sub->add_option("-a, --anim_path", opt->anim_path, "Path for anim results which includes images and videos")->required();
    // optional input if we just want to process a specific number of files
    sub->add_option("-f, --num_files", opt->maxFileNum, "Max number for files that we want to process");
    // verbose
    sub->add_option("-v, --verbose", opt->verbose, "Progress printed in terminal or not");

    sub->set_callback([opt]() 
    {
        auto total_start = chrono::high_resolution_clock::now();
        // ************************************************************************************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        // **********************************************  run LSP SKIM  **********************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        cout << "********** Running Skim **********" << endl;
        auto start = chrono::high_resolution_clock::now();
        // we need to go through all the files in the given path "input_path" and find all .czi files
        // first check this input path is a directory or a single file name
        if (checkIfDirectory(opt->czi_path))
        {
            cout << endl << "Input path " << opt->czi_path << " is valid, start processing" << endl;
        
            // get all the files in this path
            const vector<string> files = GetDirectoryFiles(opt->czi_path);
            // vector of pairs which stores each file's name and its extracted serial number
            vector< pair<int, string> > allValidFiles;
            
            // count the number of valid .czi files first
            int numFiles = 0;
            for (int i = 0; i < files.size(); i++)
            {
                // check if input file is a .czi file
                string curFile = files[i];
                int suff = curFile.rfind(".czi");

                // the file with "test" included in its name is not a .czi file that we want
                int isTest = curFile.rfind("test");

                // if this is indeed a valid .czi file
                if ( (suff != string::npos) && (suff == curFile.length() - 4) && (isTest == string::npos) )
                {
                    numFiles++;
                    // now we need to understand the sequence number of this file, which is the number between parentheses
                    int start = curFile.rfind("(");
                    int end = curFile.rfind(")");
                    int length = end - start - 1;
                    
                    // when we found the parentheses
                    if ( (start != string::npos)  && (end != string::npos) )
                    {
                        string sequenceNumString = curFile.substr(start+1, length);
                        if (is_number(sequenceNumString))
                        {
                            allValidFiles.push_back( make_pair(stoi(sequenceNumString), curFile) );
                        }                      
                        else
                        {
                            cout << "WARNING: " << sequenceNumString << " is NOT a number" << endl;
                        }
                    }
                    // the first time stamp does not come with a sequence number
                    else
                    {
                        // first time stamp start with 0
                        cout << "0 timestamp data is recorded" << endl;
                        allValidFiles.push_back( make_pair(0, curFile) );
                    }
                }
            }

            // after finding all the files, sort the allFileSerialNumber from small to large (ascending order)
            sort(allValidFiles.begin(), allValidFiles.end());

            // print out some stats
            cout << numFiles << " valid .czi files found in input path " << opt->czi_path << endl << endl;

            // sanity check
            if (numFiles != allValidFiles.size())
            {
                cout << "ERROR: Not all valid files have been recorded" << endl;
            }
                
            // generate output files by running main
            for (int i = 0; i < allValidFiles.size(); i++) 
            {                
                string nhdrFileName, xmlFileName;

                // we need to refresh both for each iteration
                opt->nhdr_out_name = "";
                opt->xml_out_name = "";

                // generate the complete path for output files
                nhdrFileName = opt->nhdr_path + GenerateOutName(allValidFiles[i].first, 3, ".nhdr");
                xmlFileName = opt->nhdr_path + GenerateOutName(allValidFiles[i].first, 3, ".xml");

                // we want to check if current potential output file already exists, if so, skip
                if (fs::exists(nhdrFileName) && fs::exists(xmlFileName))
                {
                    cout << "Both " << nhdrFileName << " and " << xmlFileName << " exist, continue to next." << endl << endl;
                    continue;
                }
                
                // run the processing program
                try 
                {
                    opt->file = allValidFiles[i].second;
                    opt->nhdr_out_name = nhdrFileName;
                    opt->xml_out_name = xmlFileName;

                    // convert startwithcorrOptions to skimOptions
                    auto opt_skim = std::make_shared<skimOptions>();
                    opt_skim->czi_path = opt->czi_path;
                    opt_skim->nhdr_path = opt->nhdr_path;
                    opt_skim->nhdr_out_name = opt->nhdr_out_name;
                    opt_skim->xml_out_name = opt->xml_out_name;
                    opt_skim->file = opt->file;
                    opt_skim->verbose = opt->verbose;
                    Skim(*opt_skim).main();
                } 
                catch(LSPException &e) 
                {
                    std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
                }
            }
        }
        // Single file mode if the input_path is a single file path
        else
        {
            // to be consistent with the above mode, also call it curFile
            string curFile = opt->czi_path;
            cout << curFile << " is not a directory, enter Single file mode" << endl;
            cout << "Checking if it is a valid .czi file" << endl;
            
            // check if input file is a .czi file
            int suff = curFile.rfind(".czi");

            if ( (suff != string::npos) && (suff != curFile.length() - 4)) 
            {
                cout << "Current input file " + curFile + " does not end with .czi, error" << endl;
                return;
            }
            else
            {
                cout << "Current input file " + curFile + " ends with .czi, process this file" << endl;
            }

            // now we need to understand the sequence number of this file, which is the number after the baseName and before the extension
            int start = curFile.rfind("(");
            int end = curFile.rfind(")");
            int length = end - start - 1;
            // when we found the parentheses
            string sequenceNumString;
            int sequenceNum;
            if ( (start != string::npos) && (end != string::npos) )
            {
                sequenceNumString = curFile.substr(start+1, length);
            
                if (is_number(sequenceNumString))
                {
                    sequenceNum = stoi(sequenceNumString);
                }                      
                else
                {
                    cout << "WARNING: " << sequenceNumString << " is NOT a number" << endl;
                }
            }
            else
            {
                sequenceNum = 0;
            }

            string nhdrFileName, xmlFileName;
            // generate the complete path for output files
            nhdrFileName = opt->nhdr_path + GenerateOutName(sequenceNum, 3, ".nhdr");
            xmlFileName = opt->nhdr_path+ GenerateOutName(sequenceNum, 3, ".xml");

            // we want to check if current potential output file already exists, if so, skip
            if (fs::exists(nhdrFileName) && fs::exists(xmlFileName))
            {
                cout << "Both " << nhdrFileName << " and " << xmlFileName << " exist, no need to process again." << endl << endl;
                return;
            }
        
            try 
            {
                opt->file = curFile;
                opt->nhdr_out_name = nhdrFileName;
                opt->xml_out_name = xmlFileName;
                
                // convert startwithcorrOptions to skimOptions
                auto opt_skim = std::make_shared<skimOptions>();
                opt_skim->czi_path = opt->czi_path;
                opt_skim->nhdr_path = opt->nhdr_path;
                opt_skim->nhdr_out_name = opt->nhdr_out_name;
                opt_skim->xml_out_name = opt->xml_out_name;
                opt_skim->file = opt->file;
                opt_skim->verbose = opt->verbose;
                Skim(*opt_skim).main();
            } 
            catch(LSPException &e) 
            {
                std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
            }

        }

        auto stop = chrono::high_resolution_clock::now(); 
        auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 
        cout << "Skim processing took " << duration.count() << " seconds" << endl << endl; 

        // ************************************************************************************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        // **********************************************  run LSP PROJ  **********************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        cout << "********** Running Proj **********" << endl;
        // first determine if input nhdr_path is valid
        if (checkIfDirectory(opt->nhdr_path))
        {
            // vector of pairs which stores each nhdr file's name and its extracted serial number
            vector< pair<int, string> > allValidFiles;

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
                    // construct options for LSP
                    auto opt_proj = make_shared<projOptions>();
                    // proj requires input czi, nhdr file path and output proj path
                    opt_proj->nhdr_path = opt->nhdr_path;
                    opt_proj->proj_path = opt->proj_path;
                    opt_proj->file_name = opt->file_name;
                    opt_proj->file_number = opt->file_number;
                    opt_proj->number_of_processed = opt->number_of_processed;
                    opt_proj->verbose = opt->verbose;

                    auto start = chrono::high_resolution_clock::now();
                    Proj(*opt_proj).main();
                    auto stop = chrono::high_resolution_clock::now(); 
                    auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 

                    opt->number_of_processed++;
                    cout << opt->number_of_processed << " out of " << opt->file_number << " files have been processed" << endl;
                    cout << "Proj processing " << opt->file_name << " took " << duration.count() << " seconds" << endl << endl; 
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
                    // construct options for LSP
                    auto opt_proj = make_shared<projOptions>();
                    // proj requires input czi, nhdr file path and output proj path
                    opt_proj->nhdr_path = opt->nhdr_path;
                    opt_proj->proj_path = opt->proj_path;
                    opt_proj->file_name = opt->file_name;
                    opt_proj->file_number = opt->file_number;
                    opt_proj->verbose = opt->verbose;

                    auto start = chrono::high_resolution_clock::now();
                    Proj(*opt_proj).main();
                    auto stop = chrono::high_resolution_clock::now(); 
                    auto duration = chrono::duration_cast<chrono::minutes>(stop - start); 
                    cout << "Proj processing " << opt->file_name << " took " << duration.count() << " minutes" << endl; 
                } 
                catch(LSPException &e) 
                {
                    std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
                }
            }
        }

        // ************************************************************************************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        // *********************************************  run LSP CORRIMG  ********************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        cout << "********** Running Corrimg **********" << endl;
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
                    // construct options for LSP
                    auto opt_corrimg = make_shared<corrimgOptions>();
                    opt_corrimg->proj_path = opt->proj_path;
                    opt_corrimg->input_file = opt->input_file;
                    opt_corrimg->image_path = opt->image_path;
                    opt_corrimg->output_file = opt->output_file;
                    opt_corrimg->kernel = opt->kernel_corrimg;
                    opt_corrimg->verbose = opt->verbose;
                    
                    cout << "Currently processing projection file " << opt_corrimg->input_file << endl;
                    auto start = chrono::high_resolution_clock::now();
                    Corrimg(*opt_corrimg).main();
                    auto stop = chrono::high_resolution_clock::now(); 
                    auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 
                    cout << "Output " << opt_corrimg->output_file << " has been saved successfully" << endl;
                    cout << "Corrimg took " << duration.count() << " seconds" << endl << endl; 
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

                    // construct options for LSP
                    auto opt_corrimg = make_shared<corrimgOptions>();
                    opt_corrimg->proj_path = opt->proj_path;
                    opt_corrimg->input_file = opt->input_file;
                    opt_corrimg->image_path = opt->image_path;
                    opt_corrimg->output_file = opt->output_file;
                    opt_corrimg->kernel = opt->kernel_corrimg;
                    opt_corrimg->verbose = opt->verbose;

                    auto start = chrono::high_resolution_clock::now();
                    Corrimg(*opt_corrimg).main();
                    auto stop = chrono::high_resolution_clock::now(); 
                    auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 
                    cout << "Output " << opt_corrimg->output_file << " has been saved successfully" << endl;
                    cout << "Corrimg took " << duration.count() << " seconds" << endl << endl; 
                }
                catch(LSPException &e)
                {
                    std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
                }
            }
        }

        // ************************************************************************************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        // ********************************************  run LSP CORRFIND  ********************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        cout << "********** Running Corrfind **********" << endl;
        try 
        {
            // check if input_path is valid, notice that there is no Single file mode for this task, has to be directory
            if (checkIfDirectory(opt->image_path))
            {
                cout << "Input path " << opt->image_path << " is valid, start processing" << endl << endl;

                // map that has key to be type (XY, XZ, YZ), and each key corresponds to a vector of pair of sequence number and string
                // unordered_map< string, vector< pair<int, string> > > inputImages;
                // vector< vector< pair<int, string> > > inputImages;
                // we know that there are two types of images, max and avg
                vector< pair<int, string> > xyImages, xzImages, yzImages;
                
                // number of images of each type
                int numXYImages = 0, numXZImages = 0, numYZImages = 0;

                // get all images from the input anim path
                const vector<string> images = GetDirectoryFiles(opt->image_path);
                for (int i = 0; i < images.size(); i++)
                {
                    // get the current file
                    string curImage = images[i];
                    
                    // check if input file is a .png file
                    int end = curImage.rfind(".png");
                    
                    // if this is indeed an image
                    if ( (end != string::npos) && (end == curImage.length() - 4) )
                    {
                        string curImageName = curImage.substr(0, end);
                        if (opt->verbose)
                            cout << "Current input file " + curImage + " ends with .png, count this image" << endl;
                        
                        // check if the type of current image is max or avg
                        int isXY = curImage.rfind("XY");
                        int isXZ = curImage.rfind("XZ");
                        int isYZ = curImage.rfind("YZ");
                        
                        // if this is a XY image
                        if ( (isXY != string::npos) && (isXZ == string::npos) && (isYZ == string::npos) )
                        {
                            numXYImages++;
                            // now we need to understand the sequence number of this file
                            int start = -1;
                            
                            // current image name without type
                            int sequenceEnd = curImage.rfind("-projXY");
                            
                            // The sequenceNumString will have zero padding, like 001
                            for (int i = 0; i < sequenceEnd; i++)
                            {
                                // we get the first position that zero padding ends
                                if (curImage[i] != '0')
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
                                int sequenceLength = sequenceEnd - start;
                                sequenceNumString = curImage.substr(start, sequenceLength);
                            }

                            if (is_number(sequenceNumString))
                            {
                                xyImages.push_back( make_pair(stoi(sequenceNumString), curImageName) );
                                //cout << sequenceNumString << ", " << curImageName << " has been added" << endl;
                            }
                            else
                            {
                                cout << "WARNING: " << sequenceNumString << " is NOT a number" << endl;
                            }
                        }
                        // if this is a XZ image
                        else if ( (isXY == string::npos) && (isXZ != string::npos) && (isYZ == string::npos) )
                        {
                            numXZImages++;
                            // now we need to understand the sequence number of this file
                            int start = -1;
                            
                            // current image name without type
                            int sequenceEnd = curImage.rfind("-projXZ");

                            // The sequenceNumString will have zero padding, like 001
                            for (int i = 0; i < sequenceEnd; i++)
                            {
                                // we get the first position that zero padding ends
                                if (curImage[i] != '0')
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
                                int sequenceLength = sequenceEnd - start;
                                sequenceNumString = curImage.substr(start, sequenceLength);
                            }

                            if (is_number(sequenceNumString))
                            {
                                xzImages.push_back( make_pair(stoi(sequenceNumString), curImageName) );
                                //cout << sequenceNumString << ", " << curImageName << " has been added" << endl;
                            }
                            else
                            {
                                cout << "WARNING: " << sequenceNumString << " is NOT a number" << endl;
                            }
                        }
                        // if this is a YZ image
                        else if ( (isXY == string::npos) && (isXZ == string::npos) && (isYZ != string::npos) )
                        {
                            numYZImages++;
                            // now we need to understand the sequence number of this file
                            int start = -1;
                            
                            // current image name without type
                            int sequenceEnd = curImage.rfind("-projYZ");

                            // The sequenceNumString will have zero padding, like 001
                            for (int i = 0; i < sequenceEnd; i++)
                            {
                                // we get the first position that zero padding ends
                                if (curImage[i] != '0')
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
                                int sequenceLength = sequenceEnd - start;
                                sequenceNumString = curImage.substr(start, sequenceLength);
                            }

                            if (is_number(sequenceNumString))
                            {
                                yzImages.push_back( make_pair(stoi(sequenceNumString), curImageName) );
                                //cout << sequenceNumString << ", " << curImageName << " has been added" << endl;
                            }
                            else
                            {
                                cout << "WARNING: " << sequenceNumString << " is NOT a number" << endl;
                            }
                        }
                        // something wrong with the input images' namings
                        else
                        {
                            cout << "Input images are NOT in the correct namings, program stops" << endl;
                            return;
                        }
                    }
                    // if this file is not an image, do nothing
                    else 
                    {
                        
                    }
                }

                // sanity check - numXYImages should equal to the size of xyImages
                if (numXYImages != xyImages.size())
                {
                    cout << "ERROR when loading xxx-projXY images due to unexpected naming, program stops" << endl;
                    return;
                }
                // sanity check - numXZImages should equal to the size of xzImages
                if (numXZImages != xzImages.size())
                {
                    cout << "ERROR when loading xxx-projXZ images due to unexpected naming, program stops" << endl;
                    return;
                }
                // sanity check - numYZImages should equal to the size of yzImages
                if (numYZImages != yzImages.size())
                {
                    cout << "ERROR when loading xxx-projYZ images due to unexpected naming, program stops" << endl;
                    return;
                }
                // sanity check - we need to have the same number of XY, XZ and YZ images
                if ( (numXYImages != numXZImages) || (numXYImages != numYZImages) || (numXZImages != numYZImages) )
                {
                    cout << "ERROR -projXY, -projXZ, and -projYZ should have the same number of images, program stops" << endl;
                    cout << "numXYImages = " << numXYImages << endl;
                    cout << "numXZImages = " << numXZImages << endl;
                    cout << "numYZImages = " << numYZImages << endl;
                    return;
                }

                // sort xyImages, xzImages and yzImages in ascending order based on their sequence numbers
                sort(xyImages.begin(), xyImages.end());
                sort(xzImages.begin(), xzImages.end());
                sort(yzImages.begin(), yzImages.end());

                cout << endl << numXYImages << " -projXY images found in input path " << opt->image_path << endl;
                cout << numXZImages << " -projXZ images found in input path " << opt->image_path << endl;
                cout << numYZImages << " -projYZ images found in input path " << opt->image_path << endl;

                opt->inputImages.push_back(xyImages);
                opt->inputImages.push_back(xzImages);
                opt->inputImages.push_back(yzImages);

                // for (int i = 0; i < opt->inputImages[0].size(); i++)
                // {
                //     for (int j = 0; j < opt->inputImages.size(); j++)
                //     {
                //         cout << opt->inputImages[j][i].second << endl;
                //     }
                // }

                // run the Corrfind
                // construct options for LSP
                auto opt_corrfind = make_shared<corrfindOptions>();
                opt_corrfind->image_path = opt->image_path;
                opt_corrfind->align_path = opt->align_path;
                opt_corrfind->output_file = opt->output_file;
                opt_corrfind->input_images = opt->input_images;
                opt_corrfind->inputImages = opt->inputImages;
                opt_corrfind->file_number = opt->file_number;
                opt_corrfind->kernel = opt->kernel_corrfind;
                opt_corrfind->verbose = opt->verbose;
                Corrfind(*opt_corrfind).main();
            }
            else
            {
                cout << "Input path is invalid, program exits" << endl;
            }
            
        } 
        catch(LSPException &e) 
        {
            std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
        }

        // ************************************************************************************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        // ********************************************  run LSP CORRNHDR  ********************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        cout << "********** Running Corrnhdr **********" << endl;
        try
        {
            // construct options for LSP
            auto opt_corrnhdr = make_shared<corrnhdrOptions>();
            opt_corrnhdr->nhdr_path = opt->nhdr_path;
            opt_corrnhdr->corr_path = opt->corr_path;
            opt_corrnhdr->new_nhdr_path = opt->new_nhdr_path;
            Corrnhdr(*opt_corrnhdr).main();
        } 
        catch (LSPException &e) 
        {
            std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
        }

        // ************************************************************************************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        // **********************************************  run LSP PROJ  **********************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        cout << "********** Running Proj with the new headers **********" << endl;
        opt_proj->number_of_processed = 0;
        // first determine if input nhdr_path is valid
        if (checkIfDirectory(opt->new_nhdr_path))
        {
            // vector of pairs which stores each nhdr file's name and its extracted serial number
            vector< pair<int, string> > allValidFiles;

            cout << endl << "nhdr input directory " << opt->new_nhdr_path << " is valid" << endl;
            
            // count the number of files
            int nhdrNum = 0;
            // get all the files from input directory
            const vector<string> files = GetDirectoryFiles(opt->new_nhdr_path);

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

            cout << nhdrNum << " .nhdr files found in input path " << opt->new_nhdr_path << endl << endl;

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
                string proj_common = opt->new_proj_path + allValidFiles[i].second + "-proj";

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
                    // construct options for LSP
                    auto opt_proj = make_shared<projOptions>();
                    // proj requires input czi, nhdr file path and output proj path
                    opt_proj->nhdr_path = opt->new_nhdr_path;
                    opt_proj->proj_path = opt->new_proj_path;
                    opt_proj->file_name = opt->file_name;
                    opt_proj->file_number = opt->file_number;
                    opt_proj->number_of_processed = opt->number_of_processed;
                    opt_proj->verbose = opt->verbose;

                    auto start = chrono::high_resolution_clock::now();
                    Proj(*opt_proj).main();
                    auto stop = chrono::high_resolution_clock::now(); 
                    auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 

                    opt->number_of_processed++;
                    cout << opt->number_of_processed << " out of " << opt->file_number << " files have been processed" << endl;
                    cout << "New Proj processing " << opt->file_name << " took " << duration.count() << " seconds" << endl << endl; 
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
            cout << opt->new_nhdr_path << " is not a directory, enter Single file mode" << endl;
            cout << "Checking if it is a valid .nhdr file" << endl;
            const string curFile = opt->new_nhdr_path;
            
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
                    // construct options for LSP
                    auto opt_proj = make_shared<projOptions>();
                    // proj requires input czi, nhdr file path and output proj path
                    opt_proj->nhdr_path = opt->new_nhdr_path;
                    opt_proj->proj_path = opt->new_proj_path;
                    opt_proj->file_name = opt->file_name;
                    opt_proj->file_number = opt->file_number;
                    opt_proj->verbose = opt->verbose;

                    auto start = chrono::high_resolution_clock::now();
                    Proj(*opt_proj).main();
                    auto stop = chrono::high_resolution_clock::now(); 
                    auto duration = chrono::duration_cast<chrono::minutes>(stop - start); 
                    cout << "New Proj processing " << opt->file_name << " took " << duration.count() << " minutes" << endl; 
                } 
                catch(LSPException &e) 
                {
                    std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
                }
            }
        }

        // ************************************************************************************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        // **********************************************  run LSP ANIM  **********************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        // ************************************************************************************************************
        cout << "********** Running Anim **********" << endl;
        // first determine if input nhdr_path is valid
        if (checkIfDirectory(opt->new_nhdr_path))
        {
            cout << "nhdr input directory " << opt->new_nhdr_path << " is valid" << endl;
            
            // count the number of files
            const vector<string> files = GetDirectoryFiles(opt->new_nhdr_path);
            
            // note that the number starts counting at 1
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
            opt->tmax = nhdrNum;
            cout << "Total number of .nhdr files that we are processing is: " << opt->tmax << endl << endl;

            try
            {
                auto opt_anim = make_shared<animOptions>();
                // anim requires input nhdr and proj paths, and output anim path
                opt_anim->nhdr_path = opt->new_nhdr_path;
                opt_anim->proj_path = opt->new_proj_path;
                opt_anim->anim_path = opt->anim_path;
                opt_anim->maxFileNum = opt->maxFileNum;
                opt_anim->fps = opt->fps;
                opt_anim->allValidFiles = opt->allValidFiles;
                opt_anim-> tmax = opt->tmax;
                opt_anim->dwn_sample = opt->dwn_sample;
                opt_anim->scale_x = opt->scale_x;
                opt_anim->scale_z = opt->scale_z;
                opt_anim->verbose = opt->verbose;

                auto start = chrono::high_resolution_clock::now();
                Anim(*opt_anim).main();
                auto stop = chrono::high_resolution_clock::now(); 
                auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 
                cout << endl << "Anim processing took " << duration.count() << " seconds" << endl << endl; 
            }
            catch(LSPException &e)
            {
                std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
            }
        }
        else
        {
            // the program also handles if input file is a single file
            cout << opt->new_nhdr_path << " is not a directory, check if it is a valid .nhdr file" << endl;
            const string curFile = opt->new_nhdr_path;

            std::cout << "Current file name is: " << curFile << endl;
            
            // check if input file is a .czi file
            int suff = curFile.rfind(".nhdr");

            if ( (suff != string::npos) || (suff != curFile.length() - 5)) 
            {
                cout << "Current input file " + curFile + " does not end with .nhdr, error" << endl;
                return;
            }
            else
            {
                cout << "Current input file " + curFile + " ends with .nhdr, process this file" << endl;

                // update file number
                opt->tmax = 0;    

                try
                {
                    auto opt_anim = make_shared<animOptions>();
                    // anim requires input nhdr and proj paths, and output anim path
                    opt_anim->nhdr_path = opt->new_nhdr_path;
                    opt_anim->proj_path = opt->new_proj_path;
                    opt_anim->anim_path = opt->anim_path;
                    opt_anim->maxFileNum = opt->maxFileNum;
                    opt_anim->fps = opt->fps;
                    opt_anim->allValidFiles = opt->allValidFiles;
                    opt_anim-> tmax = opt->tmax;
                    opt_anim->dwn_sample = opt->dwn_sample;
                    opt_anim->scale_x = opt->scale_x;
                    opt_anim->scale_z = opt->scale_z;
                    opt_anim->verbose = opt->verbose;

                    auto start = chrono::high_resolution_clock::now();
                    Anim(*opt_anim).main();
                    auto stop = chrono::high_resolution_clock::now(); 
                    auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 
                    cout << endl << "Anim processing took " << duration.count() << " seconds" << endl << endl; 
                }
                catch(LSPException &e)
                {
                    std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
                }
            }
        }
        
        
        
        // total running time for all
        auto total_stop = chrono::high_resolution_clock::now(); 
        auto total_duration = chrono::duration_cast<chrono::seconds>(total_stop - total_start); 
        cout << endl << "Total processing took " << total_duration.count() << " seconds" << endl << endl;
        
    });

}


