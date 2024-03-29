//
// Created by Robin Weiss
// Modified by Zhuokai Zhao
//

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

using namespace std;
namespace fs = boost::filesystem;

// These helper functions are also used in other files

// comparison
struct myclass {
  bool operator() (int i,int j) { return (i<j);}
} skimSmallToLarge;

// helper function that checks if a string is a number
bool is_number(const string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

// Helper function that checks if given string path is of a Directory
bool checkIfDirectory(std::string filePath)
{
	try 
    {
		// Create a Path object from given path string
		fs::path pathObj(filePath);
		// Check if path exists and is of a directory file
        if (fs::exists(pathObj) && fs::is_directory(pathObj))
            return true;
		// if (boost::filesystem::exists(pathObj) && boost::filesystem::is_directory(pathObj))
		// 	return true;
	}
	catch (fs::filesystem_error & e)
	{
		std::cerr << e.what() << std::endl;
	}
	return false;
}

// helper function that gets all the file names in a directory
vector<string> GetDirectoryFiles(const string& dir) 
{
    std::vector<std::string> files;
    std::shared_ptr<DIR> directory_ptr(opendir(dir.c_str()), [](DIR* dir){ dir && closedir(dir); });
    struct dirent *dirent_ptr;
    
    if (!directory_ptr) 
    {
        std::cout << "Error opening : "  << dir << std::endl;
        return files;
    }
    
    while ((dirent_ptr = readdir(directory_ptr.get())) != nullptr) 
    {
        files.push_back(std::string(dirent_ptr->d_name));
    }
    return files;
}

// helper function that generates 
string GenerateOutName(const int num, const int digit, const string type)
{
    // output name
    string outName;
    string inNum = to_string(num);
    if (inNum.size() > digit)
    {
        cout << "ERROR: File number exceeds the number of digits" << endl;
        return outName;
    }

    // get the output string to be all 0
    for (int i = 0; i < digit; i++)
    {
        outName += "0";
    }

    // put input number to this name
    for (int i = 0; i < inNum.size(); i++)
    {
        // For example, outName is now 000, inNum is 67
        // so digit = 3, inNum.size() = 2
        // loop 1:
        // i = 0, outName[3-2+0] = outName[1] = inNum[0] = 6 => outName = 060
        // loop 2:
        // i = 1, outName[3-2+1] = outName[2] = inNum[1] = 7 => outName = 067
        outName[digit-inNum.size()+i] = inNum[i];
    }

    // add file name type, ex, .czi
    outName = outName + type;

    return outName;
}

void setup_skim(CLI::App &app) 
{
    auto opt = std::make_shared<skimOptions>();
    // App *add_subcommand(std::string name, std::string description = "") 
    auto sub = app.add_subcommand("skim", "Utility for getting information out of CZI files. Currently for "
                                          "generating .nhdr NRRD header files to permit extracting the image "
                                          "and essential meta data from CZI file.");

    //sub->add_option("file", opt->file, "Input CZI file to process")->required();
    sub->add_option("-i, --czi_path", opt->czi_path, "Input czi files directory or single file name (single file mode)")->required();
    sub->add_option("-o, --nhdr_path", opt->nhdr_path, "Output directory where outputs will be saved at")->required();
    sub->add_option("-v, --verbose", opt->verbose, "Level of verbose debugging messages");

    // we no longer want to have base number involved
    //sub->add_option("-b, --base_name", opt->base_name, "Base name that for the sequence of input czi files, for example, the files might be named as 1811131.czi, 1811132.czi, base name is 181113")->required();
    // output name is now always set to be 000, 001, ... etc
    //sub->add_option("-n, --nhdr_out_name", opt->nhdr_out_name, "Filename for output nrrd header ending in \".nhdr\". (Default: .czi file name)");
    //sub->add_option("-x, --xml_out_name", opt->xml_out_name, "Filename for output XML metadata. (Default: .czi file name)");
    //sub->add_option("-p, --proj", opt->po, "Given a non-empty string \"foo\" axis-aligned projections saved out as foo-projXY.nrrd, foo-projXZ.nrrd, and foo-projYZ.nrrd. ");
    

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
            // vector of pairs which stores each file's extracted serial number and its name
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
                    Skim(*opt).main();
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
                Skim(*opt).main();
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
}

Skim::Skim(skimOptions const &opt)
: opt(opt), mop(airMopNew()),
    current_f(nullptr),
    proj_max_xy(nullptr),
    proj_mean_xy(nullptr),
    proj_max_xz(nullptr),
    proj_mean_xz(nullptr),
    proj_max_yz(nullptr),
    proj_mean_yz(nullptr),
    nproj_xy(nullptr),
    nproj_xz(nullptr),
    nproj_yz(nullptr),
    outputPath(opt.nhdr_path),
    cziFileName(opt.file),
    //projBaseFileName(opt.po),
    xmlFileName(opt.xml_out_name),
    nhdrFileName(opt.nhdr_out_name)
{
    // check if input path is a directory or a single file
    if (checkIfDirectory(opt.czi_path))
        cziFileName = opt.czi_path + opt.file;
    else
        cziFileName = opt.file;

    cout << "Current procesing file is: " << cziFileName << endl;
    cout << "Output .nhdr file path is: " << nhdrFileName << endl;
    cout << "Output .xml file path is: " << xmlFileName << endl;

    // if output path does not exist, create one
    if (!checkIfDirectory(outputPath))
    {
        cout << outputPath << " does not exits, but has been created" << endl;
        boost::filesystem::create_directory(outputPath);
    }

    if (opt.verbose) 
    {
        std::cout << "===========PATHS==========" << endl;
        std::cout << "Output Dir: " << outputPath << endl;
        std::cout << "===========FILES==========" << endl;
        std::cout << "CZI  : " <<  cziFileName << endl;
        std::cout << "NHDR : " <<  nhdrFileName << endl;
        std::cout << "XML  : " <<  xmlFileName << endl;
    
        // if (!projBaseFileName.empty()) 
        // {
        //     std::cout << "PROJs: " << projBaseFileName << "-projXX.nrrd" << endl;
        // }
        std::cout << "==========================" << endl;
    }

    // Open the files
    // 0_RDONLY: open for reading only
    cziFile  = open(cziFileName.c_str(), O_RDONLY);
    
    // 0_TRUNC: truncate to zero length
    // 0_CREAT: create if nonexistant
    // O_WRONLY: open for writing only
    xmlFile  = open(xmlFileName.c_str(), O_TRUNC | O_CREAT | O_WRONLY, 0666);
    if (opt.verbose)
        cout << cziFileName << " has been openned" << endl;

    
    // output file
    nhdrFile = fopen(nhdrFileName.c_str(), "w");

    // Re-used for all SID segments
    currentSID = (SID*)malloc(sizeof(SID));
    airMopAdd(mop, currentSID, airFree, airMopAlways);
}


Skim::~Skim()
{
    airMopOkay(mop);
}


void Skim::parse_file(){
    // see if we need to print
    int verbose = opt.verbose;

    //======================//
    // Parse Header Segment //
    //======================//

    // The header data for this file
    CziHeaderInfo *headerInfo = (CziHeaderInfo*)malloc(sizeof(CziHeaderInfo));
    airMopAdd(mop, headerInfo, airFree, airMopAlways);
    memset(headerInfo, 0, sizeof(CziHeaderInfo));

    // We think ZISRAWFILE is at the beginning, but who knows...so keep finding
    while(read(cziFile, currentSID, 32) == 32)
    {
        if (strcmp(currentSID->id, "ZISRAWFILE") == 0)
        {
            // Read the header data
            read(cziFile, headerInfo, 512);
            if (verbose) 
            {
                fprintf(stdout, "==========HEADER==========\n");
                fprintf(stdout, "Major      : %" PRIu32"\n", headerInfo->Major);
                fprintf(stdout, "Minor      : %" PRIu32"\n", headerInfo->Minor);
                fprintf(stdout, "FilePart   : %" PRIu32"\n", headerInfo->FilePart);
                fprintf(stdout, "MetaDataPos: %lu\n", headerInfo->MetadataPosition);
                fprintf(stdout, "UpdatePend : %" PRIu32"\n", headerInfo->UpdatePending);
                fprintf(stdout, "==========================\n\n");
            }
            if (opt.verbose)
                cout << "ZISRAWFILE has been found" << endl;
            
            break; // ZISRAWFILE has been found
        }

        // Advance to the next SID
        lseek(cziFile, currentSID->allocatedSize, SEEK_CUR);
    }


    //========================//
    // Parse MetaData Segment //
    //========================//

    // Go to the MetaData Segment
    lseek(cziFile, headerInfo->MetadataPosition, SEEK_SET);

    // Read the metadata SID
    read(cziFile, currentSID, 32);
    if (strcmp(currentSID->id, "ZISRAWMETADATA") != 0)
        throw LSPException("Metadata not where we expected it.\n", "skimczi.cpp", "Skim::parse_file");

    // Metadata for the metadata
    CziMetadataSegmentHeaderPart *metaDataSegment = (CziMetadataSegmentHeaderPart*)malloc(sizeof(CziMetadataSegmentHeaderPart));
    airMopAdd(mop, metaDataSegment, airFree, airMopAlways);
    read(cziFile, metaDataSegment, 256);
    if (verbose) 
    {
        fprintf(stdout, "=========METADATA=========\n");
        fprintf(stdout, "XmlSize    : %" PRIu32"\n", metaDataSegment->XmlSize);
        fprintf(stdout, "AttachSize : %" PRIu32"\n", metaDataSegment->AttachmentSize);
        fprintf(stdout, "==========================\n\n");
    }

    // Grab the XML
    char *xml = (char*)malloc(metaDataSegment->XmlSize);
    airMopAdd(mop, xml, airFree, airMopAlways);
    read(cziFile, xml, metaDataSegment->XmlSize);

    // Write XML to file
    write(xmlFile, xml, metaDataSegment->XmlSize);

    // Create XML doc from string in memory
    xmlDoc *doc = NULL;
    doc = xmlReadMemory(xml, metaDataSegment->XmlSize, "noname.xml", NULL, 0);
    if (doc == NULL)
        throw LSPException("Could not parse XML\n", "skimczi.cpp", "Skim::parse_file");

    // Get the root element node
    xmlNode *root_element = NULL;
    root_element = xmlDocGetRootElement(doc);

    // Parse XML for image dimensions
    dims = (ImageDims*)malloc(sizeof(ImageDims));
    airMopAdd(mop, dims, airFree, airMopAlways);
    memset(dims, 0, sizeof(ImageDims));
    get_image_dims(root_element, dims);

    if (verbose) {
        fprintf(stdout, "====IMAGE DIMS from XML===\n");
        fprintf(stdout, "SizeX: %d\n", dims->sizeX);
        fprintf(stdout, "SizeY: %d\n", dims->sizeY);
        fprintf(stdout, "SizeZ: %d\n", dims->sizeZ);
        fprintf(stdout, "SizeC: %d\n", dims->sizeC);
        fprintf(stdout, "SizeT: %d\n", dims->sizeT);
        fprintf(stdout, "ScalingX: %0.12f\n", dims->scalingX);
        fprintf(stdout, "ScalingY: %0.12f\n", dims->scalingY);
        fprintf(stdout, "ScalingZ: %0.12f\n", dims->scalingZ);
        fprintf(stdout, "PixelType: %d\n", dims->pixelType);
        fprintf(stdout, "==========================\n\n");
    }

    if (dims->pixelType == CZIPIXELTYPE_UNDEFINED || dims->pixelType > CZIPIXELTYPE_GRAY32FLOAT){
        std::string msg = "XML indicates non-supported PixelType\n";

        airMopError(mop);

        throw LSPException(msg, "skimczi.cpp", "Skim::parse_file");
    }

    // Clean up XML parser
    xmlFreeDoc(doc);
    xmlCleanupParser();

    close(xmlFile);
}


void Skim::update_projections(){
    unsigned int sizeX = (unsigned int)dims->sizeX;
    unsigned int sizeY = (unsigned int)dims->sizeY;
    unsigned int sizeZ = (unsigned int)dims->sizeZ;

    /* update pointers to what will actually be used in this call */
    size_t off_xy = sizeX * sizeY * curr_c;

    size_t off_xz = sizeX * (curr_z + sizeZ * curr_c);

    size_t off_yz = sizeY * (curr_z + sizeZ * curr_c);

    /* all initializations */
    if (curr_z == 0)
    {
        for (auto idx = 0; idx < sizeX*sizeY; idx++){
        proj_max_xy[off_xy+ idx] = FLT_MIN;
        proj_mean_xy[off_xy+ idx] = 0;
        }
    }
    for (int x = 0; x < sizeX; x++) 
    {
        proj_max_xz[off_xz + x] = FLT_MIN;
        proj_mean_xz[off_xz + x] = 0;
    }
    for (int y = 0; y < sizeY; y++) 
    {
        proj_max_yz[off_yz + y] = FLT_MIN;
        proj_mean_yz[off_yz + y] = 0;
    }

    for (int y = 0; y < sizeY; y++)
    {
        for (int x = 0; x < sizeX; x++){
        auto idx = x + sizeX*y;
        auto cval = current_f[idx];

        if (proj_max_xy[off_xy + idx] < cval) proj_max_xy[off_xy + idx] = cval;
        proj_mean_xy[off_xy + idx] += cval / sizeZ;

        if (proj_max_xz[off_xz + x] < cval) proj_max_xz[off_xz + x] = cval;
        proj_mean_xz[off_xz + x] += cval/sizeY;

        if (proj_max_yz[off_yz + y] < cval) proj_max_yz[off_yz + y] = cval;
        proj_mean_yz[off_yz + y] += cval / sizeX;
        }
    }
}


void Skim::generate_nhdr(){
    //======================//
    // Generate NRRD Header //
    //======================//

    // NRRD Flavor
    fprintf(nhdrFile, "NRRD0006\n");

    // Pixel Type - (CZI p.23)
    if (dims->pixelType == CZIPIXELTYPE_GRAY8)
        fprintf(nhdrFile, "type: uchar\n");
    else if (dims->pixelType == CZIPIXELTYPE_GRAY16)
        fprintf(nhdrFile, "type: ushort\n");
    else if (dims->pixelType == CZIPIXELTYPE_GRAY32FLOAT)
        fprintf(nhdrFile, "type: float\n");

    // Endianness - (CZI p.7)
    fprintf(nhdrFile, "endian: little\n");

    // Data encoding - (CZI p.25)
    // NOTE: If (when reading image blocks) we find non-raw data, we abort mission
    fprintf(nhdrFile, "encoding: raw\n");

    // Image dimensions - Axes go X Y C Z
    if (dims->sizeC < 2){
        fprintf(nhdrFile, "dimension: 3\n");
        fprintf(nhdrFile, "sizes: %d %d %d\n", dims->sizeX, dims->sizeY, dims->sizeZ);
        fprintf(nhdrFile, "centers: cell cell cell\n"); // May change in future
    }
    else {
        fprintf(nhdrFile, "dimension: 4\n");
        fprintf(nhdrFile, "sizes: %d %d %d %d\n", dims->sizeX, dims->sizeY, dims->sizeC, dims->sizeZ);
        fprintf(nhdrFile, "centers: cell cell none cell\n"); // May change in future
    }

    // Coordinate system - Unclear based on CZI docs what they are using
    fprintf(nhdrFile, "space: 3D-right-handed\n");

    // Origin - For now we use 0,0,0
    fprintf(nhdrFile, "space origin: (0, 0, 0)\n");

    // Voxel spacing - Units are in meters (CZI p.52) and we convert to um
    fprintf(nhdrFile, "space units: \"um\" \"um\" \"um\"\n");

    // Space matrix - Remember to convert to um
    fprintf(nhdrFile, "space directions: (%.12f, 0, 0) (0, %.12f, 0) %s(0, 0, %.12f)\n",
            dims->scalingX / 1e-7,
            dims->scalingY / 1e-7,
            dims->sizeC < 2 ? "" : "none ",
            dims->scalingZ / 1e-7);

    // Data format
    fprintf(nhdrFile, "data file: SKIPLIST 2\n");

}

void Skim::generate_nrrd(){
    //===================//
    // Find Image Blocks //
    //===================//
    int verbose = opt.verbose;

    void *current_raw = nullptr;
    Nrrd *ncurrent = nullptr;

    if (!projBaseFileName.empty()) 
    {
        /* Allocate space for current slice in both raw and float,
        and for the projections */
        current_raw = malloc(dims->sizeX * dims->sizeY * dims->pixelSize);
        airMopAdd(mop, current_raw, airFree, airMopAlways);
        ncurrent = safe_nrrd_new(mop, (airMopper)nrrdNuke);
        nproj_xy = safe_nrrd_new(mop, (airMopper)nrrdNuke);
        nproj_xz = safe_nrrd_new(mop, (airMopper)nrrdNuke);
        nproj_yz = safe_nrrd_new(mop, (airMopper)nrrdNuke);

        size_t sizeC = dims->sizeC;
        size_t sizeX = dims->sizeX;
        size_t sizeY = dims->sizeY;
        size_t sizeZ = dims->sizeZ;
        size_t sizeP = 2; // was 3 with min, max, mean
        /* TODO: even if sizeC is 1, we still create an axis for the channels,
        because the code logic is simpler that way, but then we
        should probably remove it prior to saving out */
        nrrd_checker(nrrdAlloc_va(ncurrent, nrrdTypeFloat, 2,
                        sizeX, sizeY)
                    || nrrdAlloc_va(nproj_xy, nrrdTypeFloat, 4,
                                    sizeX, sizeY, sizeC, sizeP)
                    || nrrdAlloc_va(nproj_xz, nrrdTypeFloat, 4,
                                    sizeX, sizeZ, sizeC, sizeP)
                    || nrrdAlloc_va(nproj_yz, nrrdTypeFloat, 4,
                                    sizeY, sizeZ, sizeC, sizeP),
                    mop, "Couldn't allocate projection buffers:\n", "skimczi.cpp", "generate_nrrd");

        nrrdAxisInfoSet_va(nproj_xy, nrrdAxisInfoLabel, "x", "y", "c", "proj");
        nrrdAxisInfoSet_va(nproj_xz, nrrdAxisInfoLabel, "x", "z", "c", "proj");
        nrrdAxisInfoSet_va(nproj_yz, nrrdAxisInfoLabel, "y", "z", "c", "proj");

        current_f = (float*)ncurrent->data;
        size_t szslice = sizeX*sizeY*sizeC;
        proj_max_xy  = (float*)(nproj_xy->data) + 0;
        proj_mean_xy = (float*)(nproj_xy->data) + szslice;
        szslice = sizeX*sizeZ*sizeC;
        proj_max_xz  = (float*)(nproj_xz->data) + 0;
        proj_mean_xz = (float*)(nproj_xz->data) + szslice;
        szslice = sizeY*sizeZ*sizeC;
        proj_max_yz  = (float*)(nproj_yz->data) + 0;
        proj_mean_yz = (float*)(nproj_yz->data) + szslice;
    }

    // Rewind the file to beginning
    lseek(cziFile, 0, SEEK_SET);

    if (verbose) 
    {
        fprintf(stdout, "looking for %d slices ...", dims->sizeZ);
        fflush(stdout);
    }

/* ================================================================== */
/* 
    TODO: if size and nrrd data doesnot match, prog will crash
    Unfortunately, some data is complete!(Last step is missing)
    I deduce the size for 1 here.
    Wait to be solved.
*/
  int ctr = 0;
  size_t dataBegin;
/* ================================================================== */


  // Go hunting for image blocks
  CziSubBlockSegment *imageSubBlockHeader = (CziSubBlockSegment*)malloc(sizeof(CziSubBlockSegment));
  airMopAdd(mop, imageSubBlockHeader, airFree, airMopAlways);
  while(read(cziFile, currentSID, 32) == 32){
    // skip through file to get the image blocks
    if (strcmp(currentSID->id, "ZISRAWSUBBLOCK") == 0){
      // Remember where this segment begins
      off_t start_of_segment = lseek(cziFile, 0, SEEK_CUR) - 32;

      // Read the ImageBlock header
      read(cziFile, imageSubBlockHeader, sizeof(CziSubBlockSegment));

      // Make sure this image block has the expected PixelType
      // TODO: Also check image dimensions agree with XML?
      if (imageSubBlockHeader->PixelType != dims->pixelType)
        throw LSPException("ImageSubBlock PixelType field doesn't agree with XML\n",
                           "skimczi.cpp", "Skim::generate_nrrd");
      

      // Make sure this image block has the expected compression
      if (imageSubBlockHeader->Compression != CZICOMPRESSTYPE_RAW)
        throw LSPException("ImageSubBlock indicated unsupported compression type\n",
                          "skimczi.cpp", "Skim::generate_nrrd");

      // Channel this image slice is from
      curr_c = 0;
      curr_z = 0;
      for (int i = 0; i < imageSubBlockHeader->DimensionCount; i++){
        if (!strcmp((char *)(imageSubBlockHeader->DimensionEntries[i].Dimension), "C"))
          curr_c = imageSubBlockHeader->DimensionEntries[i].Start;
        
        if (!strcmp((char *)(imageSubBlockHeader->DimensionEntries[i].Dimension), "Z"))
          curr_z = imageSubBlockHeader->DimensionEntries[i].Start;
      }

      // Compute where the data begins
      size_t headSize = sizeof(CziSubBlockSegment) - (12 * sizeof(CziDimensionEntryDV1)) + (imageSubBlockHeader->DimensionCount * 20);
      dataBegin = imageSubBlockHeader->FilePosition + headSize + 32;

      if (verbose > 1) {
        fprintf(stdout, "======ZISRAWSUBBLOCK======\n");
        fprintf(stdout, "ID       : %s\n", currentSID->id);
        fprintf(stdout, "POS      : %ld\n", lseek(cziFile, 0, SEEK_CUR)-32);
        fprintf(stdout, "allocSize: %lu\n", currentSID->allocatedSize);
        fprintf(stdout, "usedSize : %lu\n", currentSID->usedSize);
        fprintf(stdout, "--------CONTENTS----------\n");
        fprintf(stdout, "MetadataSize   : %" PRIu32"\n",imageSubBlockHeader->MetadataSize);
        fprintf(stdout, "AttachmentSize : %" PRIu32"\n",imageSubBlockHeader->AttachmentSize);
        fprintf(stdout, "DataSize       : %lu\n",imageSubBlockHeader->DataSize);
        fprintf(stdout, "PixelType      : %" PRIu32"\n",imageSubBlockHeader->PixelType);
        fprintf(stdout, "FilePosition   : %lu\n",imageSubBlockHeader->FilePosition);
        fprintf(stdout, "FilePart       : %" PRIu32"\n",imageSubBlockHeader->FilePart);
        fprintf(stdout, "Compression    : %" PRIu32"\n",imageSubBlockHeader->Compression);
        fprintf(stdout, "DimensionCount : %" PRIu32"\n",imageSubBlockHeader->DimensionCount);

        if (verbose > 2) {
          for (int i = 0; i < imageSubBlockHeader->DimensionCount; i++){
            fprintf(stdout, "--------------------\n");
            fprintf(stdout, "DimensionID     : %s\n", imageSubBlockHeader->DimensionEntries[i].Dimension);
            fprintf(stdout, "Start           : %d\n", imageSubBlockHeader->DimensionEntries[i].Start);
            fprintf(stdout, "Size            : %d\n", imageSubBlockHeader->DimensionEntries[i].Size);
            fprintf(stdout, "StartCoordinate : %f\n", imageSubBlockHeader->DimensionEntries[i].StartCoordinate);
            fprintf(stdout, "StoredSize      : %d\n", imageSubBlockHeader->DimensionEntries[i].StoredSize);
            fprintf(stdout, "--------------------\n");
          }
        }

        fprintf(stdout, "headSize       : %ld\n", headSize);
        fprintf(stdout, "DataBegin      : %ld\n",dataBegin);
        fprintf(stdout, "==========================\n\n");
      }

      // Add entry for this slice to nhdr file
      fprintf(nhdrFile, "%ld %s\n", dataBegin, cziFileName.c_str());
      ++ctr;

      // go to the beginning of data
      lseek(cziFile, dataBegin, SEEK_SET);

      if (!projBaseFileName.empty()) {
        // read the current slice into *current_raw
        read(cziFile, current_raw, dims->sizeX * dims->sizeY * dims->pixelSize);

        // cast pixels to floats if necessary
        if (dims->pixelType == CZIPIXELTYPE_GRAY8){
          char *current = (char*)current_raw;
          for (int y = 0; y < dims->sizeY; y++)
            for (int x = 0; x < dims->sizeX; x++)
              current_f[y * dims->sizeX + x] = (float)current[y * dims->sizeX + x];
        }
        else if (dims->pixelType == CZIPIXELTYPE_GRAY16){
          short *current = (short*)current_raw;
          for (int y = 0; y < dims->sizeY; y++)
            for (int x = 0; x < dims->sizeX; x++)
              current_f[y * dims->sizeX + x] = (float)current[y * dims->sizeX + x];
        }
        else if (dims->pixelType == CZIPIXELTYPE_GRAY32FLOAT)
          memcpy(current_f, current_raw, dims->sizeX * dims->sizeY * sizeof(float));
        else
          throw LSPException("Can't deal with given pixelType\n",
                      "skimczi.cpp", "Skim::generate_nrrd");
        
        // update the projections
        update_projections();
      }
      if (verbose) {
        fprintf(stdout, " %d", curr_z);

        fflush(stdout);
      }

      // Rewind to beginning of this segment before moving on
      lseek(cziFile, start_of_segment, SEEK_SET);
    }

    // Advance to the next SID
    lseek(cziFile, currentSID->allocatedSize, SEEK_CUR);
  }

/* ================================================================== */
/* 
    TMP WORK AROUND: CONTINUE LINE 393 
*/
  while(ctr++ < dims->sizeC*dims->sizeZ)
    fprintf(nhdrFile, "%ld %s\n", dataBegin, cziFileName.c_str());
/* ================================================================== */


  if (verbose)
    fprintf(stdout, "\n");

  fclose(nhdrFile);
  close(cziFile);
}

void Skim::generate_proj(){
  if (!projBaseFileName.empty()) {
    //=======================//
    // Write out Projections //
    //=======================//
    char *projFName = AIR_CALLOC(projBaseFileName.length()
                                 + strlen("-projAA.nrrd") + 0, char);
    assert(projFName);
    airMopAdd(mop, projFName, airFree, airMopAlways);
    /* TODO: put back in the relevant per-axis stuff that can be
       sensibly set (like axis orientations).  We don't have a
       Nrrd representation of the main CZI header, and nor would it
       be very useful since nrrdWrite cannot currently generate
       SKIPLIST headers.  But once that is added to the NrrdIO struct,
       the header generation could be done by a call to nrrdWrite,
       and then we'd have a slightly cleaner way of getting/setting
       the per-axis meta data */
    int E = 0;
    if (!E) sprintf(projFName, "%s-projXY.nrrd", projBaseFileName.c_str());
    if (!E) E |= nrrdSave(projFName, nproj_xy, NULL);
    if (!E) sprintf(projFName, "%s-projXZ.nrrd", projBaseFileName.c_str());
    if (!E) E |= nrrdSave(projFName, nproj_xz, NULL);
    if (!E) sprintf(projFName, "%s-projYZ.nrrd", projBaseFileName.c_str());
    if (!E) E |= nrrdSave(projFName, nproj_yz, NULL);
    nrrd_checker(E,
                mop, "Couldn't save projections:\n",
                "skimczi.cpp", "Skim::generate_proj");
/*
    nrrdStateVerboseIO = 0;
    Nrrd *nin = safe_nrrd_load(mop, nhdrFileName); 

    if (nin) {
      Nrrd *line = safe_nrrd_new(mop, (airMopper)nrrdNuke);
      Nrrd *fline = safe_nrrd_new(mop, (airMopper)nrrdNuke);

      std::string lineFile = projBaseFileName + "-line.nrrd";

      nrrd_checker(nrrdAxesMerge(nin, nin, 0)
                    || nrrdProject(line, nin, 0, nrrdMeasureMean, nrrdTypeDefault)
                    || nrrdAxesMerge(line, line, 0)
                    || nrrdConvert(fline, line, nrrdTypeFloat)
                    || nrrdSave(lineFile.c_str(), fline, NULL),
                  mop, "Error making line:\n", "skimczi.cpp", "Skim::generate_proj");

      airMopSingleOkay(mop, line);
      airMopSingleOkay(mop, fline);
    }

    airMopSingleOkay(mop, nin);
*/
    if (opt.verbose)
      fprintf(stdout, "DONE!\n");
  }
}



void Skim::main()
{  
    //cout << "Start Skim main" << endl;
    //cout << "Processing input file " << curFile << endl;
    parse_file();
    cout << "Parsed the input file successfully" << endl;
    generate_nhdr();
    cout << "Generated nhdr header successfully" << endl;
    generate_nrrd();
    cout << "Generated nrrd file successfully" << endl << endl;
    //generate_proj();
    //cout << "Generated proj file successfully" << endl << endl << endl;
}
