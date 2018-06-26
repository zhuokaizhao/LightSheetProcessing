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
        Corrimg(*opt).main();
      } catch(LSPException &e) {
        std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
      }
  });
}


Corrimg::Corrimg(corrimgOptions const &opt): opt(opt), mop(AirMopNew()) {
  // load input file and create an empty output file
  nrrd1 = safe_nrrd_load(mop, opt.input_file);

  nrrd2 = safe_nrrd_new(mop);

  // set up nrrd kernel
  NrrdKernelSpec *kernel_spec = nrrdKernelSpecNew();
  nrrd_checker(nrrdKernelParse(&(kernel_spec->kernel), kernel_spec->parm, opt.kernel.c_str()),
              mop, "Error parsing kernel: ", "corrimg.cpp", "Corrimg::main");

  airMopAdd(mop, kernel_spec, (airMopper)nrrdKernelSpecNix, airMopAlways);

}


void Corrimg::main() {
  // get nrrd sliced and change axis order(for faster speed)
  const unsigned int axes_permute[3] = {2, 0, 1};
  nrrd_checker(nrrdSlice(nrrd2, nrrd1, 3, 1) ||
                nrrdAxesPermute(nrrd2, nrrd2, axes_permute),
              mop, "Error slicing axis: ", "corrimg.cpp", "Corrimg::main");

  // augment vals in odd channel
  size_t n = nrrdElementNumber(nrrd2);
  for (size_t i = 1; i < n; i += 2) {
    double val = 3.0*nrrdDLookup[nrrd2->type](nrrd2->data, i);
    nrrdDInsert[nrrd2->type](nrrd2->data, i, val);
  }

  // project mean val of one axis
  nrrd_checker(nrrdProject(nrrd1, nrrd2, 0, nrrdMeasureMean, nrrd2->type),
              mop, "Error projecting nrrd: ", "corrimg.cpp", "Corrimg::main");

  auto rsmc = nrrdResampleContextNew();
  airMopAdd(mop, rsmc, (airMopper)nrrdResampleContextNix, airMopAlways);

  // resample nrrd data
  nrrd_checker(nrrdResampleInputSet(rsmc, nrrd1) ||
                nrrdResampleKernelSet(rsmc, 0, kernel_spec->kernel, kernel_spec->parm) ||
                nrrdResampleSamplesSet(rsmc, 0, nrrd1->axis[0].size) ||
                nrrdResampleRangeFullSet(rsmc, 0) ||
                nrrdResampleBoundarySet(rsmc, nrrdBoundaryWeight) ||
                nrrdResampleRenormalizeSet(rsmc, AIR_TRUE) ||
                nrrdResampleKernelSet(rsmc, 1, kernel_spec->kernel, kernel_spec->parm) ||
                nrrdResampleSamplesSet(rsmc, 1, nrrd1->axis[1].size) ||
                nrrdResampleRangeFullSet(rsmc, 1) ||
                nrrdResampleExecute(rsmc, nrrd2),
              mop, "Error resampling nrrd: ", "corrimg.cpp", "Corrimg::main");


  // quantize vals from 32 to 16 bits
  nrrd_checker(nrrdQuantize(nrrd1, nrrd2, NULL, 16),
              mop, "Error quantizing nrrd: ", "corrimg.cpp", "Corrimg::main");

  // save final results
  nrrd_checker(nrrdSave(opt.output_file.c_str(), nrrd1, NULL),
              mop, "Could not save file: ", "corrimg.cpp", "Corrimg::main");

  airMopOkay(mop);
}
