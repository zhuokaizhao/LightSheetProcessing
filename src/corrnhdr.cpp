//
// Created by Jake Stover on 5/9/18.
//

#include <boost/filesystem.hpp>
#include <iostream>

#include <util.h>
#include <teem/nrrd.h>
#include "corrnhdr.h"

using namespace boost::filesystem;

void setup_corrnhdr(CLI::App &app) {
  auto opt = std::make_shared<corrnhdrOptions>();
  auto sub = app.add_subcommand("corrnhdr", "Apply the corrections calculated by corrimg and corrfind.");

  sub->add_option("-n, --num_nhdr", opt->num, "The number of the last nhdr file.");

  sub->set_callback([opt] {
      try{
        corrnhdr_main(*opt);
      } catch (LSPException &e) {
        std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
      }
  });
}

void corrnhdr_main(corrnhdrOptions const &opt) {
  // check if "/nhdr" exist for later use
  std::string dir = current_path().string() + "/nhdr/";
  if(!exists(dir))
    return ;

  auto mop = airMopNew();
  const int num = opt.num;
  dir = current_path().string() + "/reg/";
  std::string basename = "-corr1.txt";

  std::vector<std::vector<double>> shifts;  //offset from previous frame
  std::vector<std::vector<double>> offsets = {{0,0,0}}; //offset from first frame

  // read shifts and offsets from input file
  for (int i = 0; i <= num; i++) {
    path file = dir + zero_pad(i, 3) + basename;
    if (exists(file)) {
      std::ifstream inFile;
      inFile.open(file.string());

      std::vector<double> tmp;
      std::vector<double> tmp2;

      for (int j = 0; j < 3; j++) {
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

  Nrrd *offset_n = nrrdNew();
  airMopAdd(mop, offset_n, (airMopper)nrrdNix, airMopAlways);

  //save offsets into nrrd file
  if (nrrdWrap_va(offset_n, offsets.data(), nrrdTypeDouble, 2, 3, offsets.size()) ||
          nrrdSave("reg/offsets.nrrd", offset_n, NULL)) {
    char *msg;
    char *err = biffGetDone(NRRD);

    sprintf(msg, "Error creating offset nrrd: %s", err);

    airMopAdd(mop, err, airFree, airMopAlways);
    airMopError(mop);

    throw LSPException(msg, "corrnhdr.cpp", "corrnhdr_main");
  };

  // slice nrrd by x axis and median yz shifts and join them back
  Nrrd *offset_median = nrrdNew();
  Nrrd *ntmp = nrrdNew();

  auto nsize = AIR_UINT(offset_n->axis[0].size);
  auto mnout = AIR_CALLOC(nsize, Nrrd*);

  airMopAdd(mop, offset_median, (airMopper)nrrdNuke, airMopAlways);
  airMopAdd(mop, ntmp, (airMopper)nrrdNuke, airMopAlways);
  airMopAdd(mop, mnout, airFree, airMopAlways);

  for (int ni=0; ni<nsize; ni++) {
    if (nrrdSlice(ntmp, offset_n, 0, ni)) {
      char *msg;
      char *err = biffGetDone(NRRD);

      sprintf(msg, "Error slicing nrrd: %s", err);

      airMopAdd(mop, err, airFree, airMopAlways);
      airMopError(mop);

      throw LSPException(msg, "corrnhdr.cpp", "corrnhdr_main");
    }
    airMopAdd(mop, mnout[ni] = nrrdNew(), (airMopper)nrrdNuke, airMopAlways);
    if (nrrdCheapMedian(mnout[ni], ntmp, 1, 0, 2, 1.0, 256)) {
      char *msg;
      char *err = biffGetDone(NRRD);

      sprintf(msg, "Error computing median: %s", err);

      airMopAdd(mop, err, airFree, airMopAlways);
      airMopError(mop);

      throw LSPException(msg, "corrnhdr.cpp", "corrnhdr_main");
    }
  }
  if (nrrdJoin(offset_median, (const Nrrd*const*)mnout, nsize, 0, AIR_TRUE)) {
    char *msg;
    char *err = biffGetDone(NRRD);

    sprintf(msg, "Error joining median slices: %s", err);

    airMopAdd(mop, err, airFree, airMopAlways);
    airMopError(mop);

    throw LSPException(msg, "corrnhdr.cpp", "corrnhdr_main");
  }
  // copy axis info
  nrrdAxisInfoCopy(offset_median, offset_n, NULL, NRRD_AXIS_INFO_NONE);
  if (nrrdBasicInfoCopy(offset_median, offset_n,
                        NRRD_BASIC_INFO_DATA_BIT
                        | NRRD_BASIC_INFO_TYPE_BIT
                        | NRRD_BASIC_INFO_BLOCKSIZE_BIT
                        | NRRD_BASIC_INFO_DIMENSION_BIT
                        | NRRD_BASIC_INFO_CONTENT_BIT
                        | NRRD_BASIC_INFO_COMMENTS_BIT
                        | (nrrdStateKeyValuePairsPropagate
                           ? 0
                           : NRRD_BASIC_INFO_KEYVALUEPAIRS_BIT))) {
    char *msg;
    char *err = biffGetDone(NRRD);

    sprintf(msg, "Error copying nrrd info: %s", err);

    airMopAdd(mop, err, airFree, airMopAlways);
    airMopError(mop);

    throw LSPException(msg, "corrnhdr.cpp", "corrnhdr_main");
  }

  // smooth the nrrd data
  Nrrd *offset_smooth = nrrdNew();
  airMopAdd(mop, offset_smooth, (airMopper)nrrdNuke, airMopAlways);

  auto rsmc = nrrdResampleContextNew();
  airMopAdd(mop, rsmc, (airMopper)nrrdResampleContextNix, airMopAlways);


  //gaussian-blur
  double kparm[2] = {2, 3};
  if (nrrdResampleInputSet(rsmc, offset_median) ||
      nrrdResampleKernelSet(rsmc, 0, NULL, NULL) ||
      nrrdResampleBoundarySet(rsmc, nrrdBoundaryBleed) ||
      nrrdResampleRenormalizeSet(rsmc, AIR_TRUE) ||
      nrrdResampleKernelSet(rsmc, 1, nrrdKernelGaussian, kparm) ||
      nrrdResampleSamplesSet(rsmc, 1, offset_median->axis[1].size) ||
      nrrdResampleRangeFullSet(rsmc, 1) ||
      nrrdResampleExecute(rsmc, offset_smooth)) {
    char *msg;
    char *err = biffGetDone(NRRD);

    sprintf(msg, "Error resampling nrrd: %s", err);

    airMopAdd(mop, err, airFree, airMopAlways);
    airMopError(mop);

    throw LSPException(msg, "corrnhdr.cpp", "corrnhdr_main");
  }

  // create a helper nrrd struct to help smooth the boundary
  Nrrd *base = nrrdNew();
  airMopAdd(mop, base, (airMopper)nrrdNix, airMopAlways);

  std::vector<double> data = {1,1,1};

  for (int i = 0; i < 3*offset_smooth->axis[1].size-6; i++) {
    data.push_back(0);
  }
  data.insert(data.end(), {1,1,1});

  double *ptr = data.data();
  nrrdAxisInfoCopy(base, offset_smooth, NULL, NRRD_AXIS_INFO_ALL);

  if (nrrdWrap_va(base, data.data(), nrrdTypeFloat, 2, 3, offset_smooth->axis[1].size)) {
    char *msg;
    char *err = biffGetDone(NRRD);

    sprintf(msg, "Error wrapping data vector: %s", err);

    airMopAdd(mop, err, airFree, airMopAlways);
    airMopError(mop);

    throw LSPException(msg, "corrnhdr.cpp", "corrnhdr_main");
  };

  Nrrd *offset_smooth1 = nrrdNew();
  airMopAdd(mop, offset_smooth1, (airMopper)nrrdNuke, airMopAlways);

  airMopSingleOkay(mop, rsmc);
  rsmc = nrrdResampleContextNew();
  airMopAdd(mop, rsmc, (airMopper)nrrdResampleContextNix, airMopAlways);

  kparm[0] = 1.5;
  if (nrrdResampleInputSet(rsmc, base) ||
      nrrdResampleKernelSet(rsmc, 0, NULL, NULL) ||
      nrrdResampleBoundarySet(rsmc, nrrdBoundaryBleed) ||
      nrrdResampleRenormalizeSet(rsmc, AIR_TRUE) ||
      nrrdResampleKernelSet(rsmc, 1, nrrdKernelGaussian, kparm) ||
      nrrdResampleSamplesSet(rsmc, 1, base->axis[1].size) ||
      nrrdResampleRangeFullSet(rsmc, 1) ||
      nrrdResampleExecute(rsmc, offset_smooth1)) {
    char *msg;
    char *err = biffGetDone(NRRD);

    sprintf(msg, "Error resampling nrrd: %s", err);

    airMopAdd(mop, err, airFree, airMopAlways);
    airMopError(mop);

    throw LSPException(msg, "corrnhdr.cpp", "corrnhdr_main");
  }

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
  nrrdIterSetOwnNrrd(n2, offset_smooth);
  nrrdIterSetOwnNrrd(n3, offset_n);

  Nrrd *offset_smooth2 = nrrdNew();
  airMopAdd(mop, offset_smooth2, (airMopper)nrrdNuke, airMopAlways);

  nrrdArithIterTernaryOp(offset_smooth2, nrrdTernaryOpLerp, n1, n2, n3);

  //output files
  dir = current_path().string() + "/nhdr/";
  for (size_t i = 0; i <= num; i++) {
    path file = dir + zero_pad(i, 3) + ".nhdr";
    if (exists(file)) {
      Nrrd *old_nrrd = safe_load_nrrd(file.string());
      Nrrd *new_nrrd = nrrdNew();

      airMopAdd(mop, old_nrrd, (airMopper)nrrdNuke, airMopAlways);
      airMopAdd(mop, new_nrrd, (airMopper)nrrdNuke, airMopAlways);

      if (nrrdCopy(new_nrrd, old_nrrd)) {
        char *msg;
        char *err = biffGetDone(NRRD);

        sprintf(msg, "Error copying nrrd: %s", err);

        airMopAdd(mop, err, airFree, airMopAlways);
        airMopError(mop);

        throw LSPException(msg, "corrnhdr.cpp", "corrnhdr_main");
      }

      double xs = old_nrrd->axis[0].spacing;
      double ys = old_nrrd->axis[1].spacing;
      double zs = old_nrrd->axis[2].spacing;

      double x_scale = nrrdDLookup[offset_smooth2->type](offset_smooth2->data, i*3);
      double y_scale = nrrdDLookup[offset_smooth2->type](offset_smooth2->data, i*3+1);
      double z_scale = nrrdDLookup[offset_smooth2->type](offset_smooth2->data, i*3+2);

      auto *origin = AIR_MALLOC(3, double);
      origin[0] = xs*x_scale;
      origin[1]= ys*y_scale;
      origin[2]= zs*z_scale;

      nrrdSpaceOriginSet(new_nrrd, origin);
      new_nrrd->type = nrrdTypeUShort;

      std::string o_name = dir + s_num + "-corr.nhdr";
      nrrdSave(o_name.c_str(), new_nrrd, NULL);

      airMopSingleOkay(mop, old_nrrd);
      airMopSingleOkay(mop, new_nrrd);
      free(origin);
    } else {
      std::cout << "[corrnhdr] WARN: " << file.string() << " does not exist." << std::endl;
    }
  }

  airMopOkay(mop);

}