//
// Created by Jake Stover on 4/10/18.
//
#include <iostream>
#include <string>

#include "CLI11.hpp"

#include "skimczi.h"
#include "anim.h"
#include "nhdrCheck.h"
#include "untext.h"
#include "corr.h"
#include "corrimg.h"
#include "corrfind.h"
#include "corrnhdr.h"
#include "standard.h"


int main(int argc, char** argv) {
  CLI::App app{"Collection of utilities for processing of lightsheet data"};

  setup_skim(app);
  setup_anim(app);
  setup_nhdr_check(app);
  setup_untext(app);
  setup_corr(app);
  setup_corrimg(app);
  setup_corrfind(app);
  setup_corrnhdr(app);
  setup_standard(app);

  CLI11_PARSE(app, argc, argv);

  if (argc == 1) {
    std::cout << app.help();
  }

  return 0;
}