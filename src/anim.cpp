//
// Created by Jake Stover on 4/24/18.
//

#include <teem/nrrd.h>
#include <opencv2/opencv.hpp>
#include "anim.h"
#include "util.h"


void setup_anim(CLI::App &app) {
  auto opt = std::make_shared<AnimOptions>();
  auto sub = app.add_subcommand("anim", "Create animations from projection .nrrd files generated by skim.");

  sub->add_option("-t, --tmax", opt->tmax, "Number of projection files.")->required();
  sub->add_option("-p, --proj", opt->proj_path, "Where projection files are. (Default: proj/)");
  sub->add_option("-o, --anim", opt->anim_path, "Where to output anim files. (Default: anim/)");
  sub->add_option("-d, --dsample", opt->dwn_sample, "Amount by which to down-sample the data. (Default: 1)");
  sub->add_option("-x, --scalex", opt->scale_x, "Scaling on the x axis. (Default: 1.0)");
  sub->add_option("-z, --scalez", opt->scale_z, "Scaling on the z axis. (Default: 1.0)");
  sub->add_option("-v, --verbose", opt->verbose, "Print processing message or not. (Default: 0(close))");

  sub->set_callback([opt]() { 
    try{
      Anim(*opt).main();
    }
    catch(LSPException &e){
      std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
    }
  });
}


Anim::Anim(AnimOptions const &opt): opt(opt), mop(airMopNew()) {}


Anim::~Anim() {
  airMopOkay(mop);
}


void Anim::split_type(){

  double resample_xy = opt.scale_x / opt.scale_z / opt.dwn_sample;
  double resample_z = 1.0 / opt.dwn_sample;

  if(opt.verbose)
    std::cout << "Resampling Factors: resample_xy = " << resample_xy << ", resample_z = " << resample_z << std::endl;

  // slice and resample projection files
  for(int i = 0; i <= opt.tmax; i++) {
    std::string iii = zero_pad(i, 3);

    if(opt.verbose)
      std::cout << "===== " << iii << "/" << opt.tmax << " =====================" << std::endl;

    std::string xy_proj_file = opt.proj_path + iii + "-projXY.nrrd";
    std::string yz_proj_file = opt.proj_path + iii + "-projYZ.nrrd";

    Nrrd* res_rsm[2][2] = { //store {{max_z, avg_z}, {max_x, avg_x}}
                  			   {safe_nrrd_new(mop, (airMopper)nrrdNuke),
                  			    safe_nrrd_new(mop, (airMopper)nrrdNuke)},
                           {safe_nrrd_new(mop, (airMopper)nrrdNuke),
                            safe_nrrd_new(mop, (airMopper)nrrdNuke)}
			  };

    Nrrd* proj_rsm[2] = {safe_nrrd_load(mop, xy_proj_file),
                         safe_nrrd_load(mop, yz_proj_file)};
    double resample_rsm[2][2] = {{resample_xy, resample_xy},
                                  {resample_xy, resample_z}};
    double kparm[2] = {0, 0.5};
    for(auto i=0; i<2; ++i){
      auto rsmc = nrrdResampleContextNew();
      airMopAdd(mop, rsmc, (airMopper)nrrdResampleContextNix, airMopAlways);
/*
      nrrd_checker(nrrdResampleInputSet(rsmc, proj_rsm[i]) ||
                      nrrdResampleKernelSet(rsmc, 0, nrrdKernelBCCubic, kparm) ||
                      nrrdResampleSamplesSet(rsmc, 0, size_t(ceil(proj_rsm[i]->axis[1].size*resample_rsm[i][0]))) ||
                      nrrdResampleRangeFullSet(rsmc, 0) ||
                      nrrdResampleBoundarySet(rsmc, nrrdBoundaryBleed) ||
                      nrrdResampleRenormalizeSet(rsmc, AIR_TRUE) ||
                      nrrdResampleKernelSet(rsmc, 1, nrrdKernelBCCubic, kparm) ||
                      nrrdResampleSamplesSet(rsmc, 1, size_t(ceil(proj_rsm[i]->axis[1].size*resample_rsm[i][1]))) ||
                      nrrdResampleRangeFullSet(rsmc, 1) ||
                      nrrdResampleKernelSet(rsmc, 2, NULL, NULL) ||
                      nrrdResampleKernelSet(rsmc, 3, NULL, NULL) ||
                      nrrdResampleExecute(rsmc, proj_rsm[i]),
                  mop, "Error resampling nrrd:\n", "anim.cpp", "Anim::split_type");
*/

      //SWAP(AX0 AX1) for yz plane
      if(i==1)
        nrrd_checker(nrrdAxesSwap(proj_rsm[i], proj_rsm[i], 0, 1),
                    mop, "Error swaping yz axes:\n", "anim.cpp", "Anim::split_type");

      nrrd_checker(nrrdSlice(res_rsm[i][0], proj_rsm[i], 3, 0) ||
                      nrrdSlice(res_rsm[i][1], proj_rsm[i], 3, 1),
                  mop, "Error slicng nrrd:\n", "anim.cpp", "Anim::split_type");

      airMopSingleOkay(mop, rsmc);
    }

    airMopSingleOkay(mop, proj_rsm[0]);
    airMopSingleOkay(mop, proj_rsm[1]);

    max_z_frames.push_back(res_rsm[0][0]);
    avg_z_frames.push_back(res_rsm[0][1]);
    max_x_frames.push_back(res_rsm[1][0]);
    avg_x_frames.push_back(res_rsm[1][1]);
  }
}


void Anim::make_max_frame(std::vector<Nrrd*> frame, std::string direction){
  Nrrd* joined = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  Nrrd* ch0 = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  Nrrd* ch1 = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  Nrrd* bit0 = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  Nrrd* bit1 = safe_nrrd_new(mop, (airMopper)nrrdNuke);

  //join along time, slice along channel
  nrrd_checker(nrrdJoin(joined,frame.data(), frame.size(), 3, 0) ||
                nrrdSlice(ch0, joined, 2, 0) ||
                nrrdSlice(ch1, joined, 2, 1),
              mop, "Error joining/slicing nrrd:\n", "anim.cpp", "Anim::make_max_frame");

  //quantize to 8bit
  auto range = nrrdRangeNew(AIR_NAN, AIR_NAN);
  airMopAdd(mop, range, (airMopper)nrrdRangeNix, airMopAlways);
  nrrd_checker(nrrdRangePercentileFromStringSet(range, ch0,  "5%", "0.02%", 5000, true) ||
                nrrdQuantize(bit0, ch0, range, 8),
              mop, "Error quantizing ch1 nrrd:\n", "anim.cpp", "Anim::make_max_frame");

  //set brightness for ch2(and quantize to 8bit)
  nrrd_checker(nrrdArithGamma(ch1, ch1, NULL, 3) ||
                  nrrdRangePercentileFromStringSet(range, ch1, "5%", "0.01%", 5000, true) ||
                  nrrdQuantize(bit1, ch1, range, 8),
              mop, "Error quantizing ch2 nrrd:\n", "anim.cpp", "Anim::make_max_frame");
    
  //slice on time and output
  for (size_t t = 0; t < frame.size(); ++t) {
    Nrrd* bit0_t = safe_nrrd_new(mop, (airMopper)nrrdNuke);
    Nrrd* bit1_t = safe_nrrd_new(mop, (airMopper)nrrdNuke);
    std::string bit0_name = opt.anim_path + zero_pad(t, 3) + "-max-" + direction + "-0.ppm";
    std::string bit1_name = opt.anim_path + zero_pad(t, 3) + "-max-" + direction + "-1.ppm";

    if(opt.verbose)
      std::cout << "===== " << zero_pad(t, 3) << "/" << opt.tmax << " " << direction << "_max_frames"  << " =====================" << std::endl;

    nrrd_checker(nrrdSlice(bit0_t, bit0, 2, t)  ||
                  nrrdSlice(bit1_t, bit1, 2, t) ||
                  nrrdSave(bit0_name.c_str() , bit0_t, nullptr) ||
                  nrrdSave(bit1_name.c_str() , bit1_t, nullptr),
                mop, "Error saving ppm files:\n", "anim.cpp", "Anim::make_max_frame");

    airMopSingleOkay(mop, bit0_t);
    airMopSingleOkay(mop, bit1_t);
  }
}


void Anim::make_avg_frame(std::vector<Nrrd*> frame, std::string direction){
  Nrrd* joined = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  Nrrd* ch = safe_nrrd_new(mop, (airMopper)nrrdNuke);

  //join along time, slice along channel
  nrrd_checker(nrrdJoin(joined,frame.data(), frame.size(), 3, 0),
              mop, "Error joining/slicing nrrd: ", "anim.cpp", "Anim::make_avg_frame");
  //resample: gaussian blur
  auto rsmc = nrrdResampleContextNew();
  airMopAdd(mop, rsmc, (airMopper)nrrdResampleContextNix, airMopAlways);

/*
  double kparm[2] = {40,3};
  nrrd_checker(nrrdResampleInputSet(rsmc, joined) ||
                  nrrdResampleKernelSet(rsmc, 0, nrrdKernelGaussian, kparm) ||
                  nrrdResampleSamplesSet(rsmc, 0, joined->axis[0].size) ||
                  nrrdResampleRangeFullSet(rsmc, 1) ||
                  nrrdResampleBoundarySet(rsmc, nrrdBoundaryBleed) ||
                  nrrdResampleRenormalizeSet(rsmc, AIR_TRUE) ||
                  nrrdResampleKernelSet(rsmc, 1, nrrdKernelGaussian, kparm) ||
                  nrrdResampleSamplesSet(rsmc, 1, joined->axis[1].size) ||
                  nrrdResampleRangeFullSet(rsmc, 1) ||
                  nrrdResampleKernelSet(rsmc, 2, NULL, NULL) ||
                  nrrdResampleKernelSet(rsmc, 3, NULL, NULL) ||
                  nrrdResampleExecute(rsmc, ch),
              mop,  "Error resampling nrrd:\n", "anim.cpp", "Anim::make_avg_frame");
*/
  //slice on ch 
  Nrrd* ch0 = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  Nrrd* ch1 = safe_nrrd_new(mop, (airMopper)nrrdNuke);

  NrrdIter* nit1 = nrrdIterNew();
  NrrdIter* nit2 = nrrdIterNew();

  nrrdIterSetOwnNrrd(nit1, ch);
  nrrdIterSetValue(nit2, 0.5);

  nrrdArithIterBinaryOp(ch, nrrdBinaryOpMultiply, nit1, nit2);

  nrrdIterSetOwnNrrd(nit1, ch);
  nit2 = nrrdIterNix(nit2);
  nrrdIterSetOwnNrrd(nit2, joined);

  nrrdArithIterBinaryOp(joined, nrrdBinaryOpSubtract, nit2, nit1);

  nrrdSlice(ch0, joined, 2, 0);
  nrrdSlice(ch1, joined, 2, 1);

  // Assume this iterator part is nonexcept and do not add nrrdIterNix into mop because we use nix as 'reset' above.
  nrrdIterNix(nit1);
  nrrdIterNix(nit2);

  //quantize to 8bit
  Nrrd* bit0 = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  Nrrd* bit1 = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  auto range = nrrdRangeNew(AIR_NAN, AIR_NAN);
  airMopAdd(mop, range, (airMopper)nrrdRangeNix, airMopAlways);
  nrrd_checker(nrrdRangePercentileFromStringSet(range, ch0, "10%", "0.1%", 5000, true) ||
                nrrdQuantize(bit0, ch0, range, 8) ||
                nrrdRangePercentileFromStringSet(range, ch1, "10%", "0.1%", 5000, true) ||
                nrrdQuantize(bit1, ch1, range, 8),
              mop, "Error quantizing nrrd:\n", "anim.cpp", "Anim::make_avg_frame");

  //slice on time and output
  for (size_t t = 0; t < frame.size(); ++t) {
    Nrrd* bit0_t = safe_nrrd_new(mop, (airMopper)nrrdNuke);
    Nrrd* bit1_t = safe_nrrd_new(mop, (airMopper)nrrdNuke);
    std::string bit0_name = opt.anim_path + zero_pad(t, 3) + "-avg-" + direction + "-0.ppm";
    std::string bit1_name = opt.anim_path + zero_pad(t, 3) + "-avg-" + direction + "-1.ppm";

    if(opt.verbose)
      std::cout << "===== " << zero_pad(t, 3) << "/" << opt.tmax << " " << direction << "_avg_frames"  << " =====================" << std::endl;

    nrrd_checker(nrrdSlice(bit0_t, bit0, 2, t)  ||
                  nrrdSlice(bit1_t, bit1, 2, t) ||
                  nrrdSave(bit0_name.c_str() , bit0_t, nullptr) ||
                  nrrdSave(bit1_name.c_str() , bit1_t, nullptr),
                mop, "Error saving ppm files:\n", "anim.cpp", "Anim::make_avg_frame");

    airMopSingleOkay(mop, bit0_t);
    airMopSingleOkay(mop, bit1_t);
  }
}


void Anim::build_png() {
  for(auto type: {"max", "avg"}){
    for(auto i=0; i<=opt.tmax; ++i){
      if(opt.verbose)
        std::cout << "===== " << zero_pad(i, 3) << "/" << opt.tmax << " " << type << "_pngs" << " =====================" << std::endl;

      std::string base_path = opt.anim_path + zero_pad(i, 3) + "-" + type;
      Nrrd *ppm_z_0 = safe_nrrd_load(mop, base_path + "-z-0.ppm");
      Nrrd *ppm_z_1 = safe_nrrd_load(mop, base_path + "-z-1.ppm");
      Nrrd *ppm_x_0 = safe_nrrd_load(mop, base_path + "-x-0.ppm");
      Nrrd *ppm_x_1 = safe_nrrd_load(mop, base_path + "-x-1.ppm");
      std::vector<Nrrd*> ppms_z = {ppm_z_1, ppm_z_0, ppm_z_1};
      std::vector<Nrrd*> ppms_x = {ppm_x_1, ppm_x_0, ppm_x_1};

      Nrrd *nout_z = safe_nrrd_new(mop, (airMopper)nrrdNuke);
      Nrrd *nout_x = safe_nrrd_new(mop, (airMopper)nrrdNuke);
      Nrrd *tmp_nout_array[2] = {nout_z, nout_x};
      Nrrd *nout = safe_nrrd_new(mop, (airMopper)nrrdNuke);

      nrrd_checker(nrrdJoin(nout_z, ppms_z.data(), ppms_z.size(), 0, 1) ||
                    nrrdJoin(nout_x, ppms_x.data(), ppms_x.size(), 0, 1) ||
                    nrrdJoin(nout, tmp_nout_array, 2, 1, 0),
                  mop, "Error joining ppm files to png:\n", "anim.cpp", "Anim::build_png");

      std::string out_name = base_path + ".png";
      nrrd_checker(nrrdSave(out_name.c_str(), nout, nullptr), 
                  mop, "Error saving png file:\n", "anim.cpp", "Anim::build_png");

      airMopSingleOkay(mop, ppm_z_0);
      airMopSingleOkay(mop, ppm_z_1);
      airMopSingleOkay(mop, ppm_x_0);
      airMopSingleOkay(mop, ppm_x_1);
      airMopSingleOkay(mop, nout_z);
      airMopSingleOkay(mop, nout_x);
      airMopSingleOkay(mop, nout);
    }
  }
}


void Anim::build_video(){
  int tmax = opt.tmax;
  std::string base_name = opt.anim_path;

  cv::Size s = cv::imread(base_name + "000-max.png").size();
  
  for(std::string type: {"max", "avg"}){ 
    if(opt.verbose)
      std::cout << "===== " << type << "_mp4" << " =====================" << std::endl;

    std::string out_file = std::string("/home/jiawei/Desktop/") + type + ".avi";
    cv::VideoWriter vw(out_file.c_str(), cv::VideoWriter::fourcc('M','J','P','G'), 25, s, true);
    if(!vw.isOpened()) 
      std::cout << "cannot open videoWriter." << std::endl;
    for(auto i=0; i<=tmax; ++i){
      std::string name = base_name + zero_pad(i, 3) + "-" + type + ".png";
      vw << cv::imread(name);
    }
    vw.release();
  }
}


void Anim::main(){
  int verbose = opt.verbose;
/*
  if(verbose)
    std::cout << "Spliting nrrd on type dimension..." << std::endl;
  split_type();

  if(verbose)
    std::cout << "Making frames for max channel..." << std::endl;
  make_max_frame(max_x_frames, "x");
  make_max_frame(max_z_frames, "z");

  if(verbose)
    std::cout << "Making frames for avg channel..." << std::endl;
  make_avg_frame(avg_x_frames, "x");
  make_avg_frame(avg_z_frames, "z");

  if(verbose)
    std::cout << "Building pngs..." << std::endl;
  build_png();
*/
  if(verbose)
    std::cout << "Building video..." << std::endl;
  build_video();
}