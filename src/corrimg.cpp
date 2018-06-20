//
// Created by Jake Stover on 5/9/18.
//

#include <teem/nrrd.h>
#include "corrimg.h"
#include "util.h"


void setup_corrimg(CLI::App &app) {
  auto opt = std::make_shared<corrimgOptions>();
  auto sub = app.add_subcommand("corrimg", "Construct image to be used for cross correlation.");

  sub->add_option("-i, --input", opt->input_file, "Input projection nrrd.");
  sub->add_option("-o, --output", opt->output_file, "Output file name.");
  sub->add_option("-k, --kernel", opt->kernel, "Kernel to use in resampling. (Default: gauss:10,4)");

  sub->set_callback([opt]() {
      try {
        corrimg_main(*opt);
      } catch(LSPException &e) {
        std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
      }
  });
}


void corrimg_main(corrimgOptions const &opt) {
  // load input file and create an empty output file
  Nrrd *nrrd1 = safe_load_nrrd(opt.input_file);
  Nrrd *nrrd2 = nrrdNew();

  auto mop = airMopNew();
  airMopAdd(mop, nrrd1, (airMopper)nrrdNuke, airMopAlways);
  airMopAdd(mop, nrrd2, (airMopper)nrrdNuke, airMopAlways);

  // set up nrrd kernel
  NrrdKernelSpec *kernel_spec = nrrdKernelSpecNew();
  if (nrrdKernelParse(&(kernel_spec->kernel), kernel_spec->parm, opt.kernel.c_str())) {
    char *msg;
    char *err = biffGetDone(NRRD);

    sprintf(msg, "Error parsing kernel: %s", err);

    airMopAdd(mop, err, airFree, airMopAlways);
    airMopError(mop);

    throw LSPException(msg, "corrimg.cpp", "corrimg_main");
  }
  airMopAdd(mop, kernel_spec, (airMopper)nrrdKernelSpecNix, airMopAlways);

  const unsigned int axes_permute[3] = {2, 0, 1};

  // get nrrd sliced and change axis order(for faster speed)
  if (nrrdSlice(nrrd2, nrrd1, 3, 1) ||
    nrrdAxesPermute(nrrd2, nrrd2, axes_permute)) {

    char *msg;
    char *err = biffGetDone(NRRD);

    sprintf(msg, "%s", err);

    airMopAdd(mop, err, airFree, airMopAlways);
    airMopError(mop);

    throw LSPException(msg, "corrimg.cpp", "corrimg_main");
  };

  size_t n = nrrdElementNumber(nrrd2);
  for (size_t i = 1; i < n; i += 2) {
    double val = 3.0*nrrdDLookup[nrrd2->type](nrrd2->data, i);
    nrrdDInsert[nrrd2->type](nrrd2->data, i, val);
  }

  // project mean val of one axis
  if (nrrdProject(nrrd1, nrrd2, 0, nrrdMeasureMean, nrrd2->type)) {
    char *msg;
    char *err = biffGetDone(NRRD);

    sprintf(msg, "Error projecting nrrd: %s", err);

    airMopAdd(mop, err, airFree, airMopAlways);
    airMopError(mop);

    throw LSPException(msg, "corrimg.cpp", "corrimg_main");
  }

  auto rsmc = nrrdResampleContextNew();
  airMopAdd(mop, rsmc, (airMopper)nrrdResampleContextNix, airMopAlways);

  // resample nrrd data
  if (nrrdResampleInputSet(rsmc, nrrd1) ||
    nrrdResampleKernelSet(rsmc, 0, kernel_spec->kernel, kernel_spec->parm) ||
    nrrdResampleSamplesSet(rsmc, 0, nrrd1->axis[0].size) ||
    nrrdResampleRangeFullSet(rsmc, 0) ||
    nrrdResampleBoundarySet(rsmc, nrrdBoundaryWeight) ||
    nrrdResampleRenormalizeSet(rsmc, AIR_TRUE) ||
    nrrdResampleKernelSet(rsmc, 1, kernel_spec->kernel, kernel_spec->parm) ||
    nrrdResampleSamplesSet(rsmc, 1, nrrd1->axis[1].size) ||
    nrrdResampleRangeFullSet(rsmc, 1) ||
    nrrdResampleExecute(rsmc, nrrd2)) {

    char *msg;
    char *err = biffGetDone(NRRD);

    sprintf(msg, "Error resampling nrrd: %s", err);

    airMopAdd(mop, err, airFree, airMopAlways);
    airMopError(mop);

    throw LSPException(msg, "corrimg.cpp", "corrimg_main");
  }

  // quantize vals from 32 to 16 bits
  if (nrrdQuantize(nrrd1, nrrd2, NULL, 16)) {
    char *msg;
    char *err = biffGetDone(NRRD);

    sprintf(msg, "Error quantizing nrrd: %s", err);

    airMopAdd(mop, err, airFree, airMopAlways);
    airMopError(mop);

    throw LSPException(msg, "corrimg.cpp", "corrimg_main");
  };

  // save final results
  if (nrrdSave(opt.output_file.c_str(), nrrd1, NULL)) {
    char *msg;
    char *err = biffGetDone(NRRD);

    sprintf(msg, "Could not save file: %s", err);

    airMopAdd(mop, err, airFree, airMopAlways);
    airMopError(mop);

    throw LSPException(msg, "corrimg.cpp", "corrimg_main");
  };

  airMopOkay(mop);
}
