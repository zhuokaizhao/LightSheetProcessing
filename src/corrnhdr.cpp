//
// Created by Jake Stover on 5/9/18.
//

#include <boost/filesystem.hpp>
#include <iostream>

#include <teem/nrrd.h>

#include "util.h"
#include "corrnhdr.h"

using namespace boost::filesystem;

void setup_corrnhdr(CLI::App &app) {
  auto opt = std::make_shared<corrnhdrOptions>();
  auto sub = app.add_subcommand("corrnhdr", "Apply the corrections calculated by corrimg and corrfind.");

  sub->add_option("-d, --file_dir", opt->file_dir, "Where 'nhdr/' and 'reg/' are. Defualt path is working path. (Default: .)");
  sub->add_option("-n, --num_nhdr", opt->num, "The number of the last nhdr file.");

  sub->set_callback([opt] {
      try{
        Corrnhdr(*opt).main();
      } catch (LSPException &e) {
        std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
      }
  });
}


Corrnhdr::Corrnhdr(corrnhdrOptions const &opt): opt(opt), mop(airMopNew()) {
  // check if "/nhdr" exist for later use
  nhdr_dir = opt.file_dir + "/nhdr/";
  if(!exists(nhdr_dir))
    throw LSPException("Error finding 'nhdr' subdirectory.", "corrnhdr.cpp", "Corrnhdr::Corrnhdr");

  reg_dir = opt.file_dir + "/reg/";
  if(!exists(reg_dir))
    throw LSPException("Error finding 'reg' subdirectory.", "corrnhdr.cpp", "Corrnhdr::Corrnhdr");
  
  basename = "-corr1.txt";
}


Corrnhdr::~Corrnhdr(){
  //airMopOkay(mop);
}


void Corrnhdr::compute_offsets(){
  std::vector<std::vector<double>> shifts,  //offset from previous frame
                                   offsets; //offset from first frame

  // read shifts and offsets from input file
  offsets.push_back({0, 0, 0});
  for (auto i = 0; i <= opt.num; i++) {
    path file = reg_dir + zero_pad(i, 3) + basename;
    if (exists(file)) {
      std::ifstream inFile;
      inFile.open(file.string());

      std::vector<double> tmp, tmp2;
      for (auto j: {0,1,2}) {
        double x;
        inFile >> x;
        tmp.push_back(x);
        tmp2.push_back(offsets[offsets.size()-1][j] + x);
      }
      shifts.push_back(tmp);
      offsets.push_back(tmp2);

      inFile.close();
    } else {
      std::cout << "[corrnhdr] WARN: " << file.string() << " does not exist." << std::endl;
    }
  }
  offsets.erase(offsets.begin());   //Remove first entry of {0,0,0}

  //save offsets into nrrd file
  offset_origin = safe_nrrd_new(mop, (airMopper)nrrdNix);
  nrrd_checker(nrrdWrap_va(offset_origin, offsets.data(), nrrdTypeDouble, 2, 3, offsets.size()) ||
                nrrdSave((reg_dir+"offsets.nrrd").c_str(), offset_origin, NULL),
              mop, "Error creating offset nrrd: ", "corrnhdr.cpp", "Corrnhdr::compute_offsets");
}

void Corrnhdr::median_filtering(){
  // slice nrrd by x axis and median yz shifts and join them back
  offset_median = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  Nrrd *ntmp = safe_nrrd_new(mop, (airMopper)nrrdNuke);

  auto nsize = AIR_UINT(offset_origin->axis[0].size);
  auto mnout = AIR_CALLOC(nsize, Nrrd*);
  airMopAdd(mop, mnout, airFree, airMopAlways);

  for (int ni=0; ni<nsize; ni++) {
    nrrd_checker(nrrdSlice(ntmp, offset_origin, 0, ni),
                mop, "Error slicing nrrd: ", "corrnhdr.cpp", "Corrnhdr::median_filtering");

    airMopAdd(mop, mnout[ni] = nrrdNew(), (airMopper)nrrdNuke, airMopAlways);

    nrrd_checker(nrrdCheapMedian(mnout[ni], ntmp, 1, 0, 2, 1.0, 256),
                mop, "Error computing median: ", "corrnhdr.cpp", "Corrnhdr::median_filtering");
    
  }
  
  nrrd_checker(nrrdJoin(offset_median, (const Nrrd*const*)mnout, nsize, 0, AIR_TRUE), 
              mop, "Error joining median slices: ", "corrnhdr.cpp", "Corrnhdr::median_filtering");
  
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

void Corrnhdr::smooth(){
  Nrrd *offset_blur = safe_nrrd_new(mop, (airMopper)nrrdNuke);

  auto rsmc = nrrdResampleContextNew();
  airMopAdd(mop, rsmc, (airMopper)nrrdResampleContextNix, airMopAlways);

  //gaussian-blur
  double kparm[2] = {2, 3};
  nrrd_checker(nrrdResampleInputSet(rsmc, offset_median) ||
                nrrdResampleKernelSet(rsmc, 0, NULL, NULL) ||
                nrrdResampleBoundarySet(rsmc, nrrdBoundaryBleed) ||
                nrrdResampleRenormalizeSet(rsmc, AIR_TRUE) ||
                nrrdResampleKernelSet(rsmc, 1, nrrdKernelGaussian, kparm) ||
                nrrdResampleSamplesSet(rsmc, 1, offset_median->axis[1].size) ||
                nrrdResampleRangeFullSet(rsmc, 1) ||
                nrrdResampleExecute(rsmc, offset_blur),
              mop, "Error resampling nrrd: ", "corrnhdr.cpp", "Corrnhdr::smooth");
  
  // create a helper nrrd array to help smooth the boundary
  std::vector<double> data(3, 1);
  data.insert(data.end(), 3*offset_blur->axis[1].size-6, 0);
  data.insert(data.end(), 3, 1);

  Nrrd *base = safe_nrrd_new(mop, (airMopper)nrrdNix);
  nrrd_checker(nrrdWrap_va(base, data.data(), nrrdTypeFloat, 2, 3, offset_blur->axis[1].size),
              mop, "Error wrapping data vector: ", "corrnhdr.cpp", "Corrnhdr::smooth");
  nrrdAxisInfoCopy(base, offset_blur, NULL, NRRD_AXIS_INFO_ALL);

  offset_smooth1 = safe_nrrd_new(mop, (airMopper)nrrdNuke);

  airMopSingleOkay(mop, rsmc);
  rsmc = nrrdResampleContextNew();
  airMopAdd(mop, rsmc, (airMopper)nrrdResampleContextNix, airMopAlways);

  kparm[0] = 1.5;
  nrrd_checker(nrrdResampleInputSet(rsmc, base) ||
                nrrdResampleKernelSet(rsmc, 0, NULL, NULL) ||
                nrrdResampleBoundarySet(rsmc, nrrdBoundaryBleed) ||
                nrrdResampleRenormalizeSet(rsmc, AIR_TRUE) ||
                nrrdResampleKernelSet(rsmc, 1, nrrdKernelGaussian, kparm) ||
                nrrdResampleSamplesSet(rsmc, 1, base->axis[1].size) ||
                nrrdResampleRangeFullSet(rsmc, 1) ||
                nrrdResampleExecute(rsmc, offset_smooth1),
              mop, "Error resampling nrrd: ", "corrnhdr.cpp", "Corrnhdr::smooth");
  
  nrrdQuantize(offset_smooth1, offset_smooth1, NULL, 32);
  nrrdUnquantize(offset_smooth1, offset_smooth1, nrrdTypeDouble);

  // smooth the boundary
  NrrdIter *n1 = nrrdIterNew();
  NrrdIter *n2 = nrrdIterNew();
  NrrdIter *n3 = nrrdIterNew();

  airMopAdd(mop, n1, airFree, airMopAlways);
  airMopAdd(mop, n2, airFree, airMopAlways);
  airMopAdd(mop, n3, airFree, airMopAlways);

  nrrdIterSetOwnNrrd(n1, offset_smooth1);
  nrrdIterSetOwnNrrd(n2, offset_blur);
  nrrdIterSetOwnNrrd(n3, offset_origin);

  offset_smooth = safe_nrrd_new(mop, (airMopper)nrrdNuke);

  nrrdArithIterTernaryOp(offset_smooth, nrrdTernaryOpLerp, n1, n2, n3);

}


void Corrnhdr::main() {
  compute_offsets();
  median_filtering();
  smooth();

  //output files
  for (size_t i = 0; i <= opt.num; i++) {
    path file = nhdr_dir + zero_pad(i, 3) + ".nhdr";
    if (exists(file)) {
      Nrrd *old_nrrd = safe_nrrd_load(mop, file.string()),
           *new_nrrd = safe_nrrd_new(mop, (airMopper)nrrdNuke);

      nrrd_checker(nrrdCopy(new_nrrd, old_nrrd),
                  mop, "Error copying nrrd: ", "corrnhdr.cpp", "corrnhdr_main");
      
      double xs = old_nrrd->axis[0].spacing,
             ys = old_nrrd->axis[1].spacing,
             zs = old_nrrd->axis[2].spacing,
             x_scale = nrrdDLookup[offset_smooth1->type](offset_smooth1->data, i*3),
             y_scale = nrrdDLookup[offset_smooth1->type](offset_smooth1->data, i*3+1),
             z_scale = nrrdDLookup[offset_smooth1->type](offset_smooth1->data, i*3+2);

      double origin[3] = {xs*x_scale, ys*y_scale, zs*z_scale};

      nrrdSpaceOriginSet(new_nrrd, origin);
      new_nrrd->type = nrrdTypeUShort;

      std::string o_name = nhdr_dir + zero_pad(i, 3) + "-corr.nhdr";
      nrrdSave(o_name.c_str(), new_nrrd, NULL);

      airMopSingleOkay(mop, old_nrrd);
      airMopSingleOkay(mop, new_nrrd);
    }
    else
      std::cout << "[corrnhdr] WARN: " << file.string() << " does not exist." << std::endl;
  }
}