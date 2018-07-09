//
// Created by Jake Stover on 5/9/18.
//

#include <boost/filesystem.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <corr.h>

#include "util.h"

#include "corrfind.h"

void setup_corrfind(CLI::App &app) {
  auto opt = std::make_shared<corrfindOptions>();

  auto sub = app.add_subcommand("corrfind", "Computes the shift between file specified by <file_number> and <file_number>-1. Expects filenames to be %d-{XY,XZ,YX}.png");

  sub->add_option("-d, --file_dir", opt->file_dir, "Where file is. Defualt path is 'reg/' folder under working path. (Default: reg/)");
  sub->add_option("-f, --file_number", opt->file_number, "File number. This command expects files to be of the form reg/%d-{XY,XZ,YX}.png.")->required();
  sub->add_option("-o, --output", opt->output_name, "Base name to use when saving out the optimal alignemnt of images. (Default: -corr1.txt)");
  sub->add_option("-k, --kernels", opt->kernels, "Kernels to pass to lsp corr. (Default: c4hexic c4hexicd)")->expected(2);
  sub->add_option("-b, --bound", opt->bound, "Max offset to be passed to lsp corr. (Default: 10)");
  sub->add_option("-e, --epsilon", opt->epsilon, "Epsilon to be passed to lsp corr. (Default: 0.00000000000001)");

  sub->set_callback([opt]() {
      try {
        Corrfind(*opt).main();
      } catch(LSPException &e) {
        std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
      }
  });
}


Corrfind::Corrfind(corrfindOptions const &opt): opt(opt), mop(airMopNew()) {}


Corrfind::~Corrfind(){
  airMopOkay(mop);
}


void Corrfind::main() {
  std::string output_name = opt.file_dir + zero_pad(opt.file_number, 3) + opt.output_name;
  std::ofstream outfile(output_name);

  corrOptions corr_op;
  corr_op.verbosity = 0;
  corr_op.kernel = opt.kernels;
  corr_op.max_offset = opt.bound;
  corr_op.epsilon = opt.epsilon;

  if (opt.file_number == 0) {
    outfile << std::vector<double>{0, 0, 0, 0} << std::endl;
  } 
  else {
    std::vector<double> shifts;

    for (std::string proj : {"XY.png", "XZ.png", "YZ.png"}) {
      corr_op.input_images = {opt.file_dir + zero_pad(opt.file_number-1, 3) + "-" + proj,
                              opt.file_dir + zero_pad(opt.file_number, 3) + "-" + proj
                             };

      std::vector<double> shift = corr_main(corr_op);

      shifts.push_back(shift[0]);
      shifts.push_back(shift[1]);
    }

    double xx = (shifts[0] + shifts[2])/2.0;
    double yy = (shifts[1] + shifts[4])/2.0;
    double zz = (shifts[3] + shifts[5])/2.0;

    outfile << std::vector<double>{xx, yy, zz, AIR_CAST(double, opt.file_number)} << std::endl;
  }

  outfile.close();
}