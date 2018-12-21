//
// Created by Jake Stover on 5/9/18.
//

#ifndef LSP_CORR_H
#define LSP_CORR_H

#include <teem/air.h>
#include <teem/nrrd.h>

#include "CLI11.hpp"
#include "util.h"

struct corrOptions 
{
    // input_path includes images to be processed
    std::vector<std::string> input_images;
    int max_offset = 10;
    std::vector<std::string> kernel = {"box", "box"};    // This should contain kernel and derivative
    std::string output_file = "";
    double epsilon = 0.0001;
    int verbose = 0;
    int max_iters = 100;
};

void setup_corr(CLI::App &app);

std::vector<double> corr_main(corrOptions const &opt);

static double
crossCorr(const unsigned short *aa, const unsigned short *bb,
          const unsigned int sza[2], const unsigned int szb[2],
          const int off[2]);

static int
crossCorrImg(Nrrd *nout, int maxIdx[2], Nrrd *nin[2],
             int bound, int verbose,
             airArray *mop);

static double
probe(double grad[2], /* output */
      int *out, /* went outside domain */
      const double *val, unsigned int size, const double pos[2],
      double *fw, const NrrdKernelSpec *kk, const NrrdKernelSpec *dk,
      int ksup, int ilo, int ihi);

#endif //LSP_CORR_H