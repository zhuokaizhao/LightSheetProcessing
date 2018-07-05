#include <teem/nrrd.h>
#include "proj.h"
#include "util.h"

void setup_proj(CLI::App &app) {
  auto opt = std::make_shared<projOptions>();
  auto sub = app.add_subcommand("proj", "Create projection files based on nhdr file.");

  sub->add_option("-n, --file_number", opt->file_number, "Target file number. (Default: 0)");
  sub->add_option("-i, --nhdr_path", opt->nhdr_path, "Input nhdr file path. (Default: .)");
  sub->add_option("-o, --proj_path", opt->proj_path, "Where to output projection files. (Default: nhdr .)");

  sub->set_callback([opt]() { 
    try{
      Proj(*opt).main();
    }
    catch(LSPException &e){
      std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
    }
  });
}


Proj::Proj(projOptions const &opt): opt(opt), mop(airMopNew()) {
  nhdr_name = opt.nhdr_path + zero_pad(opt.file_number, 3) + ".nhdr";
  proj_common = opt.proj_path + zero_pad(opt.file_number, 3) + "-proj";
}


Proj::~Proj(){
  airMopOkay(mop);
}


void Proj::main(){
  nrrdStateVerboseIO = 0;

  Nrrd* nin = safe_nrrd_load(mop, nhdr_name);

/*
  Nrrd* blured = safe_nrrd_load(mop, nhdr_name);
  //gaussian-blur
  auto rsmc = nrrdResampleContextNew();
  airMopAdd(mop, rsmc, (airMopper)nrrdResampleContextNix, airMopAlways);
  double kparm[2] = {2, 3};
  nrrd_checker(nrrdResampleInputSet(rsmc, nin) ||
                nrrdResampleKernelSet(rsmc, 0, nrrdKernelGaussian, kparm) ||
                nrrdResampleSamplesSet(rsmc, 0, nin->axis[0].size) ||
                nrrdResampleRangeFullSet(rsmc, 0) ||
                nrrdResampleKernelSet(rsmc, 1, nrrdKernelGaussian, kparm) ||
                nrrdResampleSamplesSet(rsmc, 1, nin->axis[1].size) ||
                nrrdResampleRangeFullSet(rsmc, 1) ||
                nrrdResampleKernelSet(rsmc, 3, nrrdKernelGaussian, kparm) ||
                nrrdResampleSamplesSet(rsmc, 3, nin->axis[3].size) ||
                nrrdResampleRangeFullSet(rsmc, 3) ||
                nrrdResampleKernelSet(rsmc, 2, NULL, NULL) ||
                nrrdResampleBoundarySet(rsmc, nrrdBoundaryBleed) ||
                nrrdResampleRenormalizeSet(rsmc, AIR_TRUE) ||
                nrrdResampleExecute(rsmc, blured),
              mop, "Error resampling nrrd:\n", "proj.cpp", "Proj::main"); 
  airMopSingleOkay(mop, nin);
*/

  //xy proj
  Nrrd* nproj_xy = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  Nrrd* nproj_xy_t[2] = {safe_nrrd_new(mop, (airMopper)nrrdNuke),
                        safe_nrrd_new(mop, (airMopper)nrrdNuke)};
  nrrdProject(nproj_xy_t[0], nin, 3, nrrdMeasureMax, nrrdTypeFloat);
  nrrdProject(nproj_xy_t[1], nin, 3, nrrdMeasureMean, nrrdTypeFloat);
  nrrdJoin(nproj_xy, nproj_xy_t, 2, 3, 1);

  nrrdAxisInfoSet_va(nproj_xy, nrrdAxisInfoLabel, "x", "y", "c", "proj");
  std::string xy = proj_common + "XY.nrrd";
  nrrdSave(xy.c_str(), nproj_xy, nullptr);

  //xz proj
  Nrrd* nproj_xz = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  Nrrd* nproj_xz_t[2] = {safe_nrrd_new(mop, (airMopper)nrrdNuke),
                        safe_nrrd_new(mop, (airMopper)nrrdNuke)};
  nrrdProject(nproj_xz_t[0], nin, 1, nrrdMeasureMax, nrrdTypeFloat);
  nrrdProject(nproj_xz_t[1], nin, 1, nrrdMeasureMean, nrrdTypeFloat);
  nrrdJoin(nproj_xz, nproj_xz_t, 2, 3, 1);

  unsigned int permute[4] = {0, 2, 1, 3}; //same permute array for xz and yz coincidently
  nrrdAxesPermute(nproj_xz, nproj_xz, permute);
  nrrdAxisInfoSet_va(nproj_xz, nrrdAxisInfoLabel, "x", "z", "c", "proj");
  std::string xz = proj_common + "XZ.nrrd";
  nrrdSave(xz.c_str(), nproj_xz, nullptr);

  //yz proj
  Nrrd* nproj_yz = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  Nrrd* nproj_yz_t[2] = {safe_nrrd_new(mop, (airMopper)nrrdNuke),
                        safe_nrrd_new(mop, (airMopper)nrrdNuke)};
  nrrdProject(nproj_yz_t[0], nin, 0, nrrdMeasureMax, nrrdTypeFloat);
  nrrdProject(nproj_yz_t[1], nin, 0, nrrdMeasureMean, nrrdTypeFloat);
  nrrdJoin(nproj_yz, nproj_yz_t, 2, 3, 1);

  nrrdAxesPermute(nproj_yz, nproj_yz, permute);
  nrrdAxisInfoSet_va(nproj_yz, nrrdAxisInfoLabel, "y", "z", "c", "proj");
  std::string yz = proj_common + "YZ.nrrd";
  nrrdSave(yz.c_str(), nproj_yz, nullptr);

}
