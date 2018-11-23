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
#include <iostream>
#include <cassert>

#include <teem/air.h>
#include <teem/biff.h>
#include <teem/ell.h>

#include <fstream>
//#include <experimental/filesystem>

#include "CLI11.hpp"

#include "skimczi.h"
#include "util.h"
#include "skimczi_util.h"

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

using namespace std;
//namespace fs = std::filesystem;
namespace fs = boost::filesystem;


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

void setup_skim(CLI::App &app) {
    auto opt = std::make_shared<SkimOptions>();
    // App *add_subcommand(std::string name, std::string description = "") 
    auto sub = app.add_subcommand("skim", "Utility for getting information out of CZI files. Currently for "
                                        "generating .nhdr NRRD header files to permit extracting the image "
                                        "and essential meta data from CZI file.");

    //sub->add_option("file", opt->file, "Input CZI file to process")->required();
    sub->add_option("-i, --input_path", opt->input_path, "Input czi files path. (Default: ./czi)")->required();
    sub->add_option("-o, --output_path", opt->output_path, "Output directory where outputs will be saved at")->required();
    sub->add_option("-b, --base_name", opt->base_name, "Base name that for the sequence of input czi files, for example, the files might be named as 1811131.czi, 1811132.czi, base name is 181113")->required();
    sub->add_option("-n, --nhdr_out_name", opt->nhdr_out_name, "Filename for output nrrd header ending in \".nhdr\". (Default: .czi file name)");
    sub->add_option("-x, --xml_out_name", opt->xml_out_name, "Filename for output XML metadata. (Default: .czi file name)");
    sub->add_option("-p, --proj", opt->po, "Given a non-empty string \"foo\" axis-aligned projections saved out as foo-projXY.nrrd, foo-projXZ.nrrd, and foo-projYZ.nrrd. ");
    sub->add_option("-v, --verbose", opt->verbose, "Level of verbose debugging messages");

    sub->set_callback([opt]() 
    {
        // we need to go through all the files in the given path "input_path" and find all .czi files
        // first we need to know the number of czi files
        //boost::filesystem::path inputPath(opt->input_path);
        //if(boost::filesystem::exists(opt->input_path)) 
        if (checkIfDirectory(opt->input_path))
        {
            cout << opt->input_path << " is valid, start processing" << endl;
            fs::path inPath(opt->input_path);
            for ( auto& curFileName : boost::make_iterator_range(fs::directory_iterator(inPath), {}) )
            {
                std::cout << "Current file name is: " << curFileName << endl;
                // check if the current input file is a .czi file
                string curExtension = curFileName.path().extension().string();
                cout << "Current file extension is: " << curExtension << endl;
                if (curExtension == ".czi")
                {
                    cout << "This file is a .czi file" << endl;
                    // make this file name into opt->file
                    opt->file = curFileName.path().filename().string();  

                    // now we are trying to process this file 
                    // original program to run the file
                    try 
                    {
                        // if (opt->file == "")
                        //     cout << "Warning: Empty file name" << endl;
                        // else
                        //     cout << "Input file path: " << opt->file << endl;
                        // if (opt->no == "")
                        //     cout << "Warning: Empty nhdr" << endl;
                        // else
                        //     cout << "Output path: " << opt->no << endl;
                        Skim(*opt).main();
                    } 
                    catch(LSPException &e) 
                    {
                        std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
                    }
                }
                else
                {
                    cout << "This file is not a .czi file, next" << endl;
                }

            }
                
        }
        else
        {
            cout << opt->input_path << " is not valid, exit" << endl;
        }
    });
}

// helper function that checks if a string is a number
bool is_number(const string& s)
{
    std::string::const_iterator it = s.begin();
    while (it != s.end() && std::isdigit(*it)) ++it;
    return !s.empty() && it == s.end();
}

Skim::Skim(SkimOptions const &opt)
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
    outputPath(opt.output_path),
    cziFileName(opt.file),
    projBaseFileName(opt.po),
    xmlFileName(opt.xml_out_name),
    nhdrFileName(opt.nhdr_out_name)
{
    cout << "Output path is " << outputPath << endl;
    cout << "Input .czi file name is " << cziFileName << endl;

    // if output path does not exist, create one
    if (checkIfDirectory(outputPath))
    {
        cout << outputPath << " does not exits, but has been created" << endl;
        boost::filesystem::create_directory(outputPath);
    }
    //cout << "nhdrFileName is " << nhdrFileName << endl;
    
    // we no longer need to check here
    /*
    // check if input file is a .czi file
    u_long suff = cziFileName.rfind(".czi");
    // cout << suff << endl;
    if (!suff || (suff != cziFileName.length() - 4)) 
    {
        std::string msg = "Input file " + cziFileName + " does not end with .czi\n";

        airMopError(mop);

        throw LSPException(msg, "skimczi.cpp", "Skim::Skim");
    }
    std::string baseName = cziFileName.substr(0,suff);
    */

    string baseName = opt.base_name;
    cout << "Base Name is " << baseName << endl;

    // now we need to understand the sequence number of this file, which is the number after the baseName and before the extension
    int end = cziFileName.rfind(".czi");
    int start = cziFileName.rfind(baseName.back()) + 1;
    int length = end - start;
    std::string sequenceNumString = cziFileName.substr(start, length);

    // check if that is a valid number
    bool isNumber = is_number(sequenceNumString);
    if (!isNumber)
        return;
    
    //int sequenceNumber = stoi(sequenceNumString);

    // default names for .nhdr and .xml if not predefined
    if (nhdrFileName.empty()) 
    {
        /* the -no option was not used */
        nhdrFileName = baseName + "_" + sequenceNumString + ".nhdr";
    }
    if (xmlFileName.empty()) 
    {
        /* the -xo option was not used */
        xmlFileName = baseName + "_" + sequenceNumString + ".xml";
    }
    if (!outputPath.empty())
    {
        nhdrFileName = outputPath + nhdrFileName;
        xmlFileName = outputPath + xmlFileName;
    }

    if (opt.verbose) 
    {
        std::cout << "===========PATHS==========\n";
        std::cout << "Output Dir: " << outputPath << endl;
        std::cout << "===========FILES==========\n";
        std::cout << "CZI  : " <<  cziFileName << "\n";
        std::cout << "NHDR : " <<  nhdrFileName << "\n";
        std::cout << "XML  : " <<  xmlFileName << "\n";
    
        if (!projBaseFileName.empty()) 
        {
            std::cout << "PROJs: " << projBaseFileName << "-projXX.nrrd\n";
        }
        std::cout << "==========================\n\n";
    }

    // Open the files
    /*
    cziFile  = open(cziFileName.c_str(), O_RDONLY);
    cout << "Open result " << cziFile << endl;
    if (errno)
    {
        cout << "errno is " << errno << endl;
        throw LSPException("Error opening " + cziFileName + " : " + strerror(errno) + ".\n", "skimczi.cpp", "Skim::Skim");
    }
    */
    
    // 0_RDONLY: open for reading only
    cziFile = open(cziFileName.c_str(), O_RDONLY);
    //cziFile = 3;
    
    // 0_TRUNC: truncate to zero length
    // 0_CREAT: create if nonexistant
    // O_WRONLY: open for writing only
    xmlFile  = open(xmlFileName.c_str(), O_TRUNC | O_CREAT | O_WRONLY, 0666);
    
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
            if (verbose) {
                fprintf(stdout, "==========HEADER==========\n");
                fprintf(stdout, "Major      : %" PRIu32"\n", headerInfo->Major);
                fprintf(stdout, "Minor      : %" PRIu32"\n", headerInfo->Minor);
                fprintf(stdout, "FilePart   : %" PRIu32"\n", headerInfo->FilePart);
                fprintf(stdout, "MetaDataPos: %lu\n", headerInfo->MetadataPosition);
                fprintf(stdout, "UpdatePend : %" PRIu32"\n", headerInfo->UpdatePending);
                fprintf(stdout, "==========================\n\n");
            }
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

    if (!projBaseFileName.empty()) {
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

    if (verbose) {
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
    cout << "Start Skim main" << endl;
    parse_file();
    cout << "Finshed parsing the input file" << endl;
    generate_nhdr();
    cout << "Generated nhdr header" << endl;
    generate_nrrd();
    cout << "Generated nrrd file" << endl;
    generate_proj();
    cout << "Generated proj file" << endl;
}
