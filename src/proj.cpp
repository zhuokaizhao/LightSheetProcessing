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

  //xy proj
  Nrrd* nproj_xy = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  Nrrd* nproj_xy_t[2] = {safe_nrrd_new(mop, (airMopper)nrrdNuke),
                        safe_nrrd_new(mop, (airMopper)nrrdNuke)};
  std::string xy = proj_common + "XY.nrrd";

  // void nrrd_checker(bool status, airArray* mop, string prompt, string file, string function)
  // int nrrdProject(Nrrd *nout, const Nrrd *nin, unsigned int axis, int measr, int type)
  // int nrrdJoin(Nrrd *nout, const Nrrd *const *nin, unsigned int numNin, unsigned int axis, int incrDim);
  nrrd_checker(nrrdProject(nproj_xy_t[0], nin, 3, nrrdMeasureMax, nrrdTypeFloat) ||
                nrrdProject(nproj_xy_t[1], nin, 3, nrrdMeasureMean, nrrdTypeFloat) ||
                nrrdJoin(nproj_xy, nproj_xy_t, 2, 3, 1),
              mop, "Error building XY projection:\n", "proj.cpp", "Proj::main");

  nrrdAxisInfoSet_va(nproj_xy, nrrdAxisInfoLabel, "x", "y", "c", "proj");
  nrrd_checker(nrrdSave(xy.c_str(), nproj_xy, nullptr),
              mop, "Error saving XY projection:\n", "proj.cpp", "Proj::main");

  //xz proj
  unsigned int permute[4] = {0, 2, 1, 3}; //same permute array for xz and yz coincidently
  std::string xz = proj_common + "XZ.nrrd";
  Nrrd* nproj_xz = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  Nrrd* nproj_xz_t[2] = {safe_nrrd_new(mop, (airMopper)nrrdNuke),
                        safe_nrrd_new(mop, (airMopper)nrrdNuke)};
  nrrd_checker(nrrdProject(nproj_xz_t[0], nin, 1, nrrdMeasureMax, nrrdTypeFloat) ||
                nrrdProject(nproj_xz_t[1], nin, 1, nrrdMeasureMean, nrrdTypeFloat) ||
                nrrdJoin(nproj_xz, nproj_xz_t, 2, 3, 1) ||
                nrrdAxesPermute(nproj_xz, nproj_xz, permute),
              mop, "Error building XZ projection:\n", "proj.cpp", "Proj::main");

  nrrdAxisInfoSet_va(nproj_xz, nrrdAxisInfoLabel, "x", "z", "c", "proj");
  nrrd_checker(nrrdSave(xz.c_str(), nproj_xz, nullptr),
              mop, "Error saving XZ projection:\n", "proj.cpp", "Proj::main");

  //yz proj
  Nrrd* nproj_yz = safe_nrrd_new(mop, (airMopper)nrrdNuke);
  Nrrd* nproj_yz_t[2] = {safe_nrrd_new(mop, (airMopper)nrrdNuke),
                        safe_nrrd_new(mop, (airMopper)nrrdNuke)};
  std::string yz = proj_common + "YZ.nrrd";
  nrrd_checker(nrrdProject(nproj_yz_t[0], nin, 0, nrrdMeasureMax, nrrdTypeFloat) ||
                nrrdProject(nproj_yz_t[1], nin, 0, nrrdMeasureMean, nrrdTypeFloat) ||
                nrrdJoin(nproj_yz, nproj_yz_t, 2, 3, 1) ||
                nrrdAxesPermute(nproj_yz, nproj_yz, permute),
              mop, "Error building YZ projection:\n", "proj.cpp", "Proj::main");

  nrrdAxisInfoSet_va(nproj_yz, nrrdAxisInfoLabel, "y", "z", "c", "proj");
  nrrd_checker(nrrdSave(yz.c_str(), nproj_yz, nullptr),
              mop, "Error saving YZ projection:\n", "proj.cpp", "Proj::main");

}
