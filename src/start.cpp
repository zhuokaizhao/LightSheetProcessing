// The program runs skim, proj and anim in LSP
// Created by Zhuokai Zhao
// Contact: zhuokai@uchicago.edu

#include <teem/nrrd.h>
#include "start.h"
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

using namespace std;

namespace fs = boost::filesystem;

void start_standard_process(CLI::App &app) 
{
    auto opt = make_shared<startOptions>();
    auto sub = app.add_subcommand("start", "Stand process that includes skim, proj and anim. Limited options though.");

    // czi path
    sub->add_option("-c, --czi_path", opt->czi_path, "Path for all the czi files")->required();
    // nhdr path
    sub->add_option("-n, --nhdr_path", opt->nhdr_path, "Path for nhdr header files")->required();
    // proj path
    sub->add_option("-p, --proj_path", opt->proj_path, "Path for projection files")->required();
    // anim path
    sub->add_option("-a, --anim_path", opt->anim_path, "Path for anim results which includes images and videos")->required();
    // optional input if we just want to process a specific number of files
    sub->add_option("-f, --num_files", opt->maxFileNum, "Max number for files that we want to process");
    // verbose
    sub->add_option("-v, --verbose", opt->verbose, "Progress printed in terminal or not");

    
    auto total_start = chrono::high_resolution_clock::now();

    // **********************************************  run LSP SKIM  **********************************************
    // construct options for skim
    //auto opt = make_shared<skimOptions>();
    // skim requires input czi file input path and nhdr output path
    //opt_skim->czi_path = opt->czi_path;
    //opt_skim->nhdr_path = opt->nhdr_path;
    //opt_skim->verbose = opt->verbose;

    sub->set_callback([opt]() 
    {
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

                    // convert startOptions to skimOptions
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
                
                // convert startOptions to skimOptions
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
        cout << "Skim processing time is " << duration.count() << " seconds" << endl << endl; 
        
    });

    // **********************************************  run LSP PROJ  **********************************************
    // construct options for LSP
    // auto opt_proj = make_shared<projOptions>();
    // // proj requires input czi, nhdr file path and output proj path
    // opt_proj->nhdr_path = opt->nhdr_path;
    // opt_proj->proj_path = opt->proj_path;
    // opt_proj->verbose = opt->verbose;

    // sub->set_callback([opt]() 
    // {
    //     // vector of pairs which stores each nhdr file's name and its extracted serial number
    //     vector< pair<int, string> > allValidFiles;

    //     // first determine if input nhdr_path is valid
    //     if (checkIfDirectory(opt->nhdr_path))
    //     {
    //         cout << endl << "nhdr input directory " << opt->nhdr_path << " is valid" << endl;
            
    //         // count the number of files
    //         int nhdrNum = 0;
    //         // get all the files from input directory
    //         const vector<string> files = GetDirectoryFiles(opt->nhdr_path);

    //         // since files include .nhdr and .xml file in pairs, we want to count individual number
    //         for (const string curFile : files) 
    //         {
    //             // check if input file is a .nhdr file
    //             int nhdr_suff = curFile.rfind(".nhdr");
    //             if ( (nhdr_suff != string::npos) && (nhdr_suff == curFile.length() - 5))
    //             {
    //                 if (opt->verbose)
    //                     cout << "Current input file " + curFile + " ends with .nhdr, count this file" << endl;
                    
    //                 nhdrNum++;
    //                 // now we need to understand the sequence number of this file
    //                 int end = curFile.rfind(".nhdr");
    //                 int start = -1;
    //                 // current file name without type
    //                 string curFileName = curFile.substr(0, end);

    //                 // The sequenceNumString will have zero padding, like 001
    //                 for (int i = 0; i < end; i++)
    //                 {
    //                     // we get the first position that zero padding ends
    //                     if (curFile[i] != '0')
    //                     {
    //                         start = i;
    //                         break;
    //                     }
    //                 }
        
    //                 string sequenceNumString;
    //                 // for the case that it is just 000 which represents the initial time stamp
    //                 if (start == -1)
    //                 {
    //                     sequenceNumString = "0";
    //                 }
    //                 else
    //                 {
    //                     int length = end - start;
    //                     sequenceNumString = curFile.substr(start, length);
    //                 }

    //                 if (is_number(sequenceNumString))
    //                 {
    //                     allValidFiles.push_back( make_pair(stoi(sequenceNumString), curFileName) );
    //                 }
    //                 else
    //                 {
    //                     cout << "WARNING: " << sequenceNumString << " is NOT a number" << endl;
    //                 }
                        
    //             }

    //         }

    //         // after finding all the files, sort the allFileSerialNumber in ascending order
    //         sort(allValidFiles.begin(), allValidFiles.end());

    //         cout << nhdrNum << " .nhdr files found in input path " << opt->nhdr_path << endl << endl;

    //         // sanity check
    //         if (nhdrNum != allValidFiles.size())
    //         {
    //             cout << "ERROR: Not all valid files have been recorded" << endl;
    //         }

    //         // update file number
    //         opt->file_number = nhdrNum;
    //         cout << "Starting second loop for processing" << endl << endl;

    //         // another loop to process files
    //         for (int i = 0; i < allValidFiles.size(); i++)
    //         {   
    //             string proj_common = allValidFiles[i].second + "-proj";

    //             // we want to know if this proj file exists (processed before), don't overwrite it
    //             string proj_name_1 = proj_common + "XY.nrrd";
    //             string proj_name_2 = proj_common + "XZ.nrrd";
    //             string proj_name_3 = proj_common + "YZ.nrrd";
    //             fs::path projPath_1(proj_name_1);
    //             fs::path projPath_2(proj_name_2);
    //             fs::path projPath_3(proj_name_3);

    //             // when all three exists, skip this file
    //             if (fs::exists(projPath_1) && fs::exists(projPath_2) && fs::exists(proj_name_3))
    //             {
    //                 cout << "All " << proj_name_1 << ", " << proj_name_2 << ", " << proj_name_3 << " exist, continue to next." << endl;
    //                 opt->number_of_processed++;
    //                 cout << opt->number_of_processed << " out of " << opt->file_number << " files have been processed" << endl << endl;
    //                 continue;
    //             }

    //             // note that file name from allValidFiles does not include nhdr path
    //             opt->file_name = allValidFiles[i].second + ".nhdr";
    //             try
    //             {
    //                 auto start = chrono::high_resolution_clock::now();
    //                 Proj(*opt).main();
    //                 auto stop = chrono::high_resolution_clock::now(); 
    //                 auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 
    //                 opt->number_of_processed++;
    //                 cout << opt->number_of_processed << " out of " << opt->file_number << " files have been processed" << endl;
    //                 cout << "Processing " << opt->file_name << " took " << duration.count() << " seconds" << endl << endl; 
    //             }
    //             catch(LSPException &e)
    //             {
    //                 std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
    //             }
    //         }
    //     }
    //     // single file case
    //     else
    //     {
    //         // the program also handles if input file is a single file
    //         cout << opt->nhdr_path << " is not a directory, enter Single file mode" << endl;
    //         cout << "Checking if it is a valid .nhdr file" << endl;
    //         const string curFile = opt->nhdr_path;
            
    //         // check if input file is a .nhdr file
    //         int suff = curFile.rfind(".nhdr");

    //         if ( (suff == string::npos) || (suff != curFile.length() - 5) ) 
    //         {
    //             cout << "Current input file " + curFile + " does not end with .nhdr, error" << endl;
    //             return;
    //         }
    //         else
    //         {
    //             cout << "Current input file " + curFile + " ends with .nhdr, process this file" << endl;

    //             // update file number
    //             opt->file_number = 1;   
    //             opt->file_name = curFile;         
    //             try 
    //             {
    //                 auto start = chrono::high_resolution_clock::now();
    //                 Proj(*opt).main();
    //                 auto stop = chrono::high_resolution_clock::now(); 
    //                 auto duration = chrono::duration_cast<chrono::minutes>(stop - start); 
    //                 cout << "Processing " << opt->file_name << " took " << duration.count() << " minutes" << endl; 
    //             } 
    //             catch(LSPException &e) 
    //             {
    //                 std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
    //             }
    //         }
    //     }
        
    // });

    // **********************************************  run LSP ANIM  **********************************************
    auto opt_anim = make_shared<animOptions>();
    // anim requires input nhdr and proj paths, and output anim path
    opt_anim->nhdr_path = opt->nhdr_path;
    opt_anim->proj_path = opt->proj_path;
    opt_anim->anim_path = opt->anim_path;
    opt_anim->verbose = opt->verbose;

    //Anim::Anim(*opt).main();
    auto total_stop = chrono::high_resolution_clock::now(); 
    auto total_duration = chrono::duration_cast<chrono::seconds>(total_stop - total_start); 
    cout << endl << "Processing took " << total_duration.count() << " seconds" << endl << endl; 


}


