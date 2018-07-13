//
// Created by Jake Stover on 5/9/18.
//
#include "CLI11.hpp"

#include <teem/nrrd.h>
#include <fftw3.h>
#include <cmath>

#include "untext.h"
#include "util.h"

void setup_untext(CLI::App &app) {
  auto opt = std::make_shared<untextOptions>();
  auto sub = app.add_subcommand("untext", "Remove grid texture from a projection.");

  sub->add_option("-i, --input", opt->input, "Input file.")->required();
  sub->add_option("-o, --onput", opt->output, "Output file.")->required();

  sub->set_callback([opt]() {
      try {
        Untext(*opt).main();
      } catch(LSPException &e) {
        std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
      }
  });
}


Untext::Untext(untextOptions const &opt)
: opt(opt), mop(airMopNew()), nin(safe_nrrd_load(mop, opt.input)) {
  //TODO: check if nin is a vaild input

  szx = nin->axis[0].size;
  szy = nin->axis[1].size;

  //init fft plan
  in = AIR_CALLOC(szx*szy, fftwf_complex);
  out = AIR_CALLOC(szx*szy, fftwf_complex);
  airMopAdd(mop, in, airFree, airMopAlways);
  airMopAdd(mop, out, airFree, airMopAlways);

  p = fftwf_plan_dft_2d(szx, szy, in, out, FFTW_BACKWARD, FFTW_MEASURE);
  ip = fftwf_plan_dft_2d(szx, szy, out, in, FFTW_FORWARD, FFTW_MEASURE);
}


Untext::~Untext(){
  fftwf_destroy_plan(p);
  fftwf_destroy_plan(ip);

  airMopOkay(mop);
}


void Untext::masking(fftwf_complex *data){
  float mask_width = 0.005;
  float center_radius = 0.05;
  float soften = 0.000001;

  for(size_t i=szx*center_radius; i<szx*(1-center_radius); ++i)
    for(size_t j=0; j<szy*mask_width; ++j){
      data[j*szx+i][0] = soften;
      data[j*szx+i][1] = soften;
      data[(szy-1-j)*szx+i][0] = soften;
      data[(szy-1-j)*szx+i][1] = soften;
    }
  for(size_t i=0; i<szx*mask_width; ++i)
    for(size_t j=szy*center_radius; j<szy*(1-center_radius); ++j){
      data[j*szx+i][0] = soften;
      data[j*szx+i][1] = soften;
      data[j*szx+szx-1+i][0] = soften;
      data[j*szx+szx-1+i][1] = soften;
    }

}


Nrrd* Untext::untext_slice(Nrrd* proj, int ch, int type){
  //slicing the input nrrd file
  Nrrd* n1 = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  Nrrd* n2 = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  nrrd_checker(nrrdSlice(n1, proj, 3, type) || //proj type
                nrrdSlice(n2, n1, 2, ch),  //ch
              mop, "Error slicing nrrd file:\n", "untext.cpp", "Untext::untext_slice");

  //original is xy slice data
  float* original = (float*)n2->data;

  //read nrrdFloats into fftw_complexs
  for(auto i=0; i<szx*szy; ++i){
    in[i][0] = original[i];
    in[i][1] = 0;
  }

  //do fft
  fftwf_execute(p);

  //masking
  masking(out);

  //do ifft
  fftwf_execute(ip);

  //set fftw_complexs back to floats
  float* res = AIR_CALLOC(szx*szy, float);
  airMopAdd(mop, res, airFree, airMopAlways);
  for(auto i=0; i<szx*szy; ++i)
    res[i] = in[i][0];

  //build output nrrd array
  Nrrd* n3 = safe_nrrd_new(mop, (airMopper)nrrdNix);
  nrrd_checker(nrrdWrap_va(n3, res, nrrdTypeFloat, 2, szx, szy),
              mop, "Error wrapping data into nrrd array:\n", "untext.cpp", "Untext::untext_slice");
  return n3;
}


void Untext::main(){
  //add paddings
  size_t const old_szx = szx;
  size_t const old_szy = szy;
  szx = static_cast<size_t>(std::pow(2, std::ceil(std::log2(szx))));
  szy = static_cast<size_t>(std::pow(2, std::ceil(std::log2(szy))));

  auto nt = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  double kparm[3] = {1, 0, 0.5};
  auto rsmc1 = nrrdResampleContextNew();      
  airMopAdd(mop, rsmc1, (airMopper)nrrdResampleContextNix, airMopAlways);
  
  nrrd_checker(nrrdResampleInputSet(rsmc1, nin) ||
                nrrdResampleKernelSet(rsmc1, 0, nrrdKernelBCCubic, kparm) ||
                nrrdResampleSamplesSet(rsmc1, 0, szx) ||
                nrrdResampleRangeFullSet(rsmc1, 0) ||
                nrrdResampleKernelSet(rsmc1, 1, nrrdKernelBCCubic, kparm) ||
                nrrdResampleSamplesSet(rsmc1, 1, szy) ||
                nrrdResampleRangeFullSet(rsmc1, 1) ||
                nrrdResampleKernelSet(rsmc1, 2, nullptr, nullptr) ||
                nrrdResampleKernelSet(rsmc1, 3, nullptr, nullptr) ||
                nrrdResampleBoundarySet(rsmc1, nrrdBoundaryBleed) ||
                nrrdResampleRenormalizeSet(rsmc1, AIR_TRUE) ||
                nrrdResampleExecute(rsmc1, nt),
              mop, "Error resampling(padding) nrrd:\n", "untext.cpp", "Untext::main");


  //untext every slice and join them together
  Nrrd* type[2] = {safe_nrrd_new(mop, (airMopper)nrrdNuke), safe_nrrd_new(mop, (airMopper)nrrdNuke)};
  for(auto i: {0, 1}){
    Nrrd* type_t[2] = {untext_slice(nt, 0, i), untext_slice(nt, 1, i)};
    nrrd_checker(nrrdJoin(type[i], type_t, 2, 2, 1),
                mop, "Error joining untexted nrrd channel slices:\n", "untext.cpp", "Untext::main");
  }

  auto joined = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  nrrd_checker(nrrdJoin(joined, type, 2, 3, 1),
              mop, "Error joining untexted nrrd type slices:\n", "untext.cpp", "Untext::main");

  //delete paddings
  Nrrd* res = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  auto rsmc2 = nrrdResampleContextNew();      
  airMopAdd(mop, rsmc2, (airMopper)nrrdResampleContextNix, airMopAlways);

  nrrd_checker(nrrdResampleInputSet(rsmc2, joined) ||
                nrrdResampleKernelSet(rsmc2, 0, nrrdKernelBCCubic, kparm) ||
                nrrdResampleSamplesSet(rsmc2, 0, old_szx) ||
                nrrdResampleRangeFullSet(rsmc2, 0) ||
                nrrdResampleKernelSet(rsmc2, 1, nrrdKernelBCCubic, kparm) ||
                nrrdResampleSamplesSet(rsmc2, 1, old_szy) ||
                nrrdResampleRangeFullSet(rsmc2, 1) ||
                nrrdResampleKernelSet(rsmc2, 2, nullptr, nullptr) ||
                nrrdResampleKernelSet(rsmc2, 3, nullptr, nullptr) ||
                nrrdResampleBoundarySet(rsmc2, nrrdBoundaryBleed) ||
                nrrdResampleRenormalizeSet(rsmc2, AIR_TRUE) ||
                nrrdResampleExecute(rsmc2, res),
              mop, "Error resampling(padding) nrrd:\n", "untext.cpp", "Untext::main");

  //save new proj file.
  nrrd_checker(nrrdSave(opt.output.c_str(), res, nullptr),
              mop, "Error saving untexted nrrd type slices:\n", "untext.cpp", "Untext::main");

}