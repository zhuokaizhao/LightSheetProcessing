//
// Created by Jake Stover on 5/9/18.
//
#include <CLI11.hpp>

#include "untext.h"
#include "util.h"

void setup_untext(CLI::App &app) {
  auto opt = std::make_shared<untextOptions>();
  auto sub = app.add_subcommand("untext", "Remove grid texture from a projection.");

  sub->add_option("-f, --filename", opt->filename, "Path to directory containing *-line.nrrd files.");

  sub->set_callback([opt]() {
      try {
        untext_main(*opt);
      } catch(LSPException &e) {
        std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
      }
  });
}

void untext_main(untextOptions const & opt) {
  std::string filename = opt.filename;

}
