//
// Created by Robin Weiss
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

#include <teem/air.h>
#include <teem/biff.h>
#include <teem/ell.h>

#include <CLI11.hpp>

#include "skimczi.h"
#include "util.h"
#include "skimczi_util.h"

static void
update_projections(ImageDims *dims,     // Image metadata
                   int curr_c,          // current channel
                   int curr_z,          // current z slice
                   float *current,      // current slice data
                   float *proj_max_xy,  // max xy projection
        // float *proj_min_xy,  // min xy projection
                   float *proj_mean_xy, // mean xy projection
                   float *proj_max_xz,  // max xz projection
        // float *proj_min_xz,  // min xz projection
                   float *proj_mean_xz, // mean xz projection
                   float *proj_max_yz,  // max yz projection
        // float *proj_min_yz,  // min yz projection
                   float *proj_mean_yz  // mean yz projection
){
  unsigned int idx;  // temp for current index
  float cval;        // current value

  unsigned int sizeX = (unsigned int)dims->sizeX;
  unsigned int sizeY = (unsigned int)dims->sizeY;
  unsigned int sizeZ = (unsigned int)dims->sizeZ;

  /* update pointers to what will actually be used in this call */
  size_t off = sizeX * sizeY * curr_c;
  // proj_min_xy += off;
  proj_max_xy += off;
  proj_mean_xy += off;
  off = sizeX * (curr_z + sizeZ * curr_c);
  proj_max_xz += off;
  // proj_min_xz += off;
  proj_mean_xz += off;
  off = sizeY * (curr_z + sizeZ * curr_c);
  proj_max_yz += off;
  // proj_min_yz += off;
  proj_mean_yz += off;

  /* all initializations */
  if (curr_z == 0){
    for (idx = 0; idx < sizeX*sizeY; idx++){
      proj_max_xy[idx] = FLT_MIN;
      // proj_min_xy[idx] = FLT_MAX;
      proj_mean_xy[idx] = 0;
    }
  }
  for (int x = 0; x < sizeX; x++) {
    proj_max_xz[x] = FLT_MIN;
    // proj_min_xz[x] = FLT_MAX;
    proj_mean_xz[x] = 0;
  }
  for (int y = 0; y < sizeY; y++) {
    proj_max_yz[y] = FLT_MIN;
    // proj_min_yz[y] = FLT_MAX;
    proj_mean_yz[y] = 0;
  }

  for (int y = 0; y < sizeY; y++){
    for (int x = 0; x < sizeX; x++){
      idx = x + sizeX*y;
      cval = current[idx];

      if (proj_max_xy[idx] < cval) proj_max_xy[idx] = cval;
      // if (proj_min_xy[idx] > cval) proj_min_xy[idx] = cval;
      proj_mean_xy[idx] += cval / sizeZ;

      if (proj_max_xz[x] < cval) proj_max_xz[x] = cval;
      // if (proj_min_xz[x] > cval) proj_min_xz[x] = cval;
      proj_mean_xz[x] += cval/sizeY;

      if (proj_max_yz[y] < cval) proj_max_yz[y] = cval;
      // if (proj_min_yz[y] > cval) proj_min_yz[y] = cval;
      proj_mean_yz[y] += cval / sizeX;
    }
  }

  return;
}

void setup_skim(CLI::App &app) {
  auto opt = std::make_shared<SkimOptions>();
  auto sub = app.add_subcommand("skim", "Utility for getting information out of CZI files. Currently for "
                                        "generating .nhdr NRRD header files to permit extracting the image "
                                        "and essential meta data from CZI file.");

  sub->add_option("file", opt->file, "Input CZI file to process")->required();
  sub->add_option("-v, --verbose", opt->verbose, "Level of verbose debugging messages");
  sub->add_option("-n, --nhdr", opt->no, "Filename for output nrrd header ending in \".nhdr\". (Default: .czi file name)");
  sub->add_option("-x, --xml", opt->xo, "Filename for output XML metadata. (Default: .czi file name)");
  sub->add_option("-p, --proj", opt->po, "Given a non-empty string \"foo\" axis-aligned projections saved out as foo-projXY.nrrd, foo-projXZ.nrrd, and foo-projYZ.nrrd. ");

  sub->set_callback([opt]() {
    try {
      skim_main(*opt);
    } catch(LSPException &e) {
      std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
    }
  });
}

int skim_main(SkimOptions const &opt){

  std::string cziFileName = opt.file,
       projBaseFileName = opt.po,
       _xmlFileName = opt.xo,
       _nhdrFileName = opt.no;

  int verbose = opt.verbose;

  airArray *mop = airMopNew();

  u_long suff = cziFileName.rfind(".czi");
  if (!suff || (suff != cziFileName.length() - 4)) {
    std::string msg = "Input file " + cziFileName + " does not end with .czi\n";

    airMopError(mop);

    throw LSPException(msg, "skimczi.cpp", "skim_main");
  }

  std::string baseName = cziFileName.substr(0,suff);

  std::string nhdrFileName;
  if (_nhdrFileName.empty()) {
    /* the -no option was not used */
    nhdrFileName = baseName + ".nhdr";
  } else {
    /* this was explicitly given with -no, so use it */
    nhdrFileName = _nhdrFileName;
  }
  /* HEY copy and paste from above */
  std::string xmlFileName;
  if (_xmlFileName.empty()) {
    /* the -xo option was not used */
    xmlFileName = baseName + ".xml";
  } else {
    /* this was explicitly given with -xo, so use it */
    xmlFileName = _xmlFileName;
  }

  if (verbose) {
    std::cout << "===========FILES==========\n";
    std::cout << "CZI  : " <<  cziFileName << "\n";
    std::cout << "NHDR : " <<  nhdrFileName << "\n";
    std::cout << "XML  : " <<  xmlFileName << "\n";
    if (projBaseFileName.length()) {
      std::cout << "PROJs: " << projBaseFileName << "-projXX.nrrd\n";
    }
    std::cout << "==========================\n\n";
  }

  // Open the files
  int cziFile  = open(cziFileName.c_str(), O_RDONLY);
  if (errno){
    std::string msg = "Error opening " + cziFileName + " : " + strerror(errno) + ".\n";

    airMopError(mop);

    throw LSPException(msg, "skimczi.cpp", "skim_main");
  }
  FILE * nhdrFile = fopen(nhdrFileName.c_str(), "w");
  int xmlFile  = open(xmlFileName.c_str(), O_TRUNC | O_CREAT | O_WRONLY, 0666);

  // Re-used for all SID segments
  SID *currentSID = (SID*)malloc(sizeof(SID));
  airMopAdd(mop, currentSID, airFree, airMopAlways);

  //======================//
  // Parse Header Segment //
  //======================//

  // The header data for this file
  CziHeaderInfo *headerInfo = (CziHeaderInfo*)malloc(sizeof(CziHeaderInfo));
  airMopAdd(mop, headerInfo, airFree, airMopAlways);
  memset(headerInfo, 0, sizeof(CziHeaderInfo));

  // We think ZISRAWFILE is at the beginning, but who knows...
  while(read(cziFile, currentSID, 32) == 32){

    if (strcmp(currentSID->id, "ZISRAWFILE") == 0){
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
  if (strcmp(currentSID->id, "ZISRAWMETADATA") != 0){
    std::string msg = "Metadata not where we expected it.\n";

    airMopError(mop);

    throw LSPException(msg, "skimczi.cpp", "skim_main");
  }

  // Metadata for the metadata
  CziMetadataSegmentHeaderPart *metaDataSegment = (CziMetadataSegmentHeaderPart*)malloc(sizeof(CziMetadataSegmentHeaderPart));
  airMopAdd(mop, metaDataSegment, airFree, airMopAlways);
  read(cziFile, metaDataSegment, 256);
  if (verbose) {
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
  if (doc == NULL) {
    std::string msg = "Could not parse XML\n";

    airMopError(mop);

    throw LSPException(msg, "skimczi.cpp", "skim_main");
  }

  // Get the root element node
  xmlNode *root_element = NULL;
  root_element = xmlDocGetRootElement(doc);

  // Parse XML for image dimensions
  ImageDims *dims = (ImageDims*)malloc(sizeof(ImageDims));
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

    throw LSPException(msg, "skimczi.cpp", "skim_main");
  }

  // Clean up XML parser
  xmlFreeDoc(doc);
  xmlCleanupParser();


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


  //===================//
  // Find Image Blocks //
  //===================//

  void *current_raw = NULL;
  float *current_f = NULL;
  float *proj_max_xy = NULL;
  // float *proj_min_xy = NULL;
  float *proj_mean_xy = NULL;
  float *proj_max_xz = NULL;
  // float *proj_min_xz = NULL;
  float *proj_mean_xz = NULL;
  float *proj_max_yz = NULL;
  // float *proj_min_yz = NULL;
  float *proj_mean_yz = NULL;
  Nrrd *ncurrent = NULL, *nproj_xy = NULL, *nproj_xz = NULL, *nproj_yz = NULL;
  if (projBaseFileName.length()) {
    /* Allocate space for current slice in both raw and float,
       and for the projections */
    current_raw = malloc(dims->sizeX * dims->sizeY * dims->pixelSize);
    ncurrent = nrrdNew();
    nproj_xy = nrrdNew();
    nproj_xz = nrrdNew();
    nproj_yz = nrrdNew();
    airMopAdd(mop, ncurrent, (airMopper)nrrdNuke, airMopAlways);
    airMopAdd(mop, nproj_xy, (airMopper)nrrdNuke, airMopAlways);
    airMopAdd(mop, nproj_xz, (airMopper)nrrdNuke, airMopAlways);
    airMopAdd(mop, nproj_yz, (airMopper)nrrdNuke, airMopAlways);
    size_t sizeC = dims->sizeC;
    size_t sizeX = dims->sizeX;
    size_t sizeY = dims->sizeY;
    size_t sizeZ = dims->sizeZ;
    size_t sizeP = 2; // was 3 with min, max, mean
    /* TODO: even if sizeC is 1, we still create an axis for the channels,
       because the code logic is simpler that way, but then we
       should probably remove it prior to saving out */
    if (nrrdAlloc_va(ncurrent, nrrdTypeFloat, 2,
                     sizeX, sizeY)
        || nrrdAlloc_va(nproj_xy, nrrdTypeFloat, 4,
                        sizeX, sizeY, sizeC, sizeP)
        || nrrdAlloc_va(nproj_xz, nrrdTypeFloat, 4,
                        sizeX, sizeZ, sizeC, sizeP)
        || nrrdAlloc_va(nproj_yz, nrrdTypeFloat, 4,
                        sizeY, sizeZ, sizeC, sizeP)) {
      char *err = biffGetDone(NRRD);
      char *msg;
      sprintf(msg, "Couldn't allocate projection buffers:\n%s", err);

      airMopAdd(mop, err, airFree, airMopAlways);
      airMopError(mop);

      throw LSPException(msg, "skimczi.cpp", "skim_main");
    }
    nrrdAxisInfoSet_va(nproj_xy, nrrdAxisInfoLabel, "x", "y", "c", "proj");
    nrrdAxisInfoSet_va(nproj_xz, nrrdAxisInfoLabel, "x", "z", "c", "proj");
    nrrdAxisInfoSet_va(nproj_yz, nrrdAxisInfoLabel, "y", "z", "c", "proj");
    current_f = (float*)ncurrent->data;
    size_t szslice = sizeX*sizeY*sizeC;
    proj_max_xy  = (float*)(nproj_xy->data) + 0;
    // proj_min_xy  = (float*)(nproj_xy->data) + szslice;
    proj_mean_xy = (float*)(nproj_xy->data) + szslice;
    szslice = sizeX*sizeZ*sizeC;
    proj_max_xz  = (float*)(nproj_xz->data) + 0;
    // proj_min_xz  = (float*)(nproj_xz->data) + szslice;
    proj_mean_xz = (float*)(nproj_xz->data) + szslice;
    szslice = sizeY*sizeZ*sizeC;
    proj_max_yz  = (float*)(nproj_yz->data) + 0;
    // proj_min_yz  = (float*)(nproj_yz->data) + szslice;
    proj_mean_yz = (float*)(nproj_yz->data) + szslice;
  }

  // Rewind the file to beginning
  lseek(cziFile, 0, SEEK_SET);

  if (verbose) {
    fprintf(stdout, "looking for %d slices ...", dims->sizeZ);
    fflush(stdout);
  }
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
      if (imageSubBlockHeader->PixelType != dims->pixelType){
        std::string msg = "ImageSubBlock PixelType field doesn't agree with XML\n";

        airMopError(mop);

        throw LSPException(msg, "skimczi.cpp", "skim_main");
      }

      // Make sure this image block has the expected compression
      if (imageSubBlockHeader->Compression != CZICOMPRESSTYPE_RAW){
        std::string msg = "ImageSubBlock indicated unsupported compression type\n";

        airMopError(mop);

        throw LSPException(msg, "skimczi.cpp", "skim_main");
      }

      // Channel this image slice is from
      int curr_c = 0;
      int curr_z = 0;
      for (int i = 0; i < imageSubBlockHeader->DimensionCount; i++){
        if (!strcmp((char *)(imageSubBlockHeader->DimensionEntries[i].Dimension), "C")){
          curr_c = imageSubBlockHeader->DimensionEntries[i].Start;
        }
        if (!strcmp((char *)(imageSubBlockHeader->DimensionEntries[i].Dimension), "Z")){
          curr_z = imageSubBlockHeader->DimensionEntries[i].Start;
        }
      }

      // Compute where the data begins
      size_t headSize = sizeof(CziSubBlockSegment) - (12 * sizeof(CziDimensionEntryDV1)) + (imageSubBlockHeader->DimensionCount * 20);
      size_t dataBegin = imageSubBlockHeader->FilePosition + headSize + 32;

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

      // go to the beginning of data
      lseek(cziFile, dataBegin, SEEK_SET);

      if (projBaseFileName.length()) {
        // read the current slice into *current_raw
        read(cziFile, current_raw, dims->sizeX * dims->sizeY * dims->pixelSize);

        // cast pixels to floats if necessary
        if (dims->pixelType == CZIPIXELTYPE_GRAY8){
          char *current = (char*)current_raw;
          for (int y = 0; y < dims->sizeY; y++){
            for (int x = 0; x < dims->sizeX; x++){
              current_f[y * dims->sizeX + x] = (float)current[y * dims->sizeX + x];
            }
          }
        }
        else if (dims->pixelType == CZIPIXELTYPE_GRAY16){
          short *current = (short*)current_raw;
          for (int y = 0; y < dims->sizeY; y++){
            for (int x = 0; x < dims->sizeX; x++){
              current_f[y * dims->sizeX + x] = (float)current[y * dims->sizeX + x];
            }
          }
        }
        else if (dims->pixelType == CZIPIXELTYPE_GRAY32FLOAT){
          memcpy(current_f, current_raw, dims->sizeX * dims->sizeY * sizeof(float));
        }
        else {
          std::string msg = "Can't deal with given pixelType\n";

          airMopError(mop);

          throw LSPException(msg, "skimczi.cpp", "skim_main");
        }
        // update the projections
        update_projections(dims, curr_c, curr_z, current_f,
                           proj_max_xy,
                // proj_min_xy,
                           proj_mean_xy,
                           proj_max_xz,
                // proj_min_xz,
                           proj_mean_xz,
                           proj_max_yz,
                // proj_min_yz,
                           proj_mean_yz);
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
  if (verbose) {
    fprintf(stdout, "\n");
  }

  if (projBaseFileName.length()) {
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
    if (E) {
      char *err = biffGetDone(NRRD);
      char *msg;

      sprintf(msg, "Couldn't save projections:\n%s", err);

      airMopAdd(mop, err, airFree, airMopAlways);
      airMopError(mop);

      throw LSPException(msg, "skimczi.cpp", "skim_main");
    }
  }

  fclose(nhdrFile);
  close(cziFile);
  close(xmlFile);

  //nrrdStateVerboseIO = 0;
  Nrrd *nin = safe_load_nrrd(nhdrFileName);
  airMopAdd(mop, nin, (airMopper)nrrdNix, airMopAlways);

  if (nin) {
    Nrrd *line = nrrdNew();
    Nrrd *fline = nrrdNew();

    airMopAdd(mop, line, (airMopper)nrrdNix, airMopAlways);
    airMopAdd(mop, fline, (airMopper)nrrdNix, airMopAlways);
    std::string lineFile = projBaseFileName + "-line.nrrd";

    if (nrrdAxesMerge(nin, nin, 0)
      || nrrdProject(line, nin, 0, nrrdMeasureMean, nrrdTypeDefault)
      || nrrdAxesMerge(line, line, 0)
      || nrrdConvert(fline, line, nrrdTypeFloat)
      || nrrdSave(lineFile.c_str(), fline, NULL)) {
      char *msg;
      char *err = biffGetDone(NRRD);

      sprintf(msg, "Error making line: %s", err);

      airMopAdd(mop, err, airFree, airMopAlways);
      airMopError(mop);

      throw LSPException(msg, "skimczi.cpp", "skim_main");
    }
  }

  if (verbose) {
    fprintf(stdout, "DONE!\n");
  }

  airMopOkay(mop);
  return 0;
}
