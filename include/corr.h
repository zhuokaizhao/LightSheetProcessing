//! \file corr.h
//! \author Jake Stover
//! \date 2018-05-09
//! \brief Compute the corr val for a 2D squre dataset
//! \brief rewrite by Jiawei Jiang at 2018-06-26

#ifndef LSP_CORR_H
#define LSP_CORR_H

#include <teem/air.h>
#include <teem/nrrd.h>

#include "CLI11.hpp"
#include "util.h"

struct corrOptions {
    std::vector<std::string> input_images;
    int max_offset = 10;
    std::vector<std::string> kernel = {"box", "box"};    // This should contain kernel and derivative
    std::string output_file = "";
    double epsilon = 0.0001;
    int verbosity = 1;
    int max_iters = 100;
};

void setup_corr(CLI::App &app);

class Corr{
public:
  Corr(corrOptions const &opt);
  ~Corr();

  void main();
  std::vector<double> get_shift();

private:
  double cross_corr(const unsigned short *aa, const unsigned short *bb,
          const unsigned int sza[2], const unsigned int szb[2],
          const int off[2]);
  void cross_corrImg();
  double probe(double grad[2], /* output */
        const double pos[2],
        int *out /* went outside domain */);
  void set_kernel();
  void compute_shift();

  corrOptions const opt;
  airArray* mop;
  std::vector<double> shift;

  /* helper vars */
  Nrrd *nin[2], *nout;
  NrrdKernelSpec *kk[2]; 
  int ksup, ilo, ihi, maxIdx[2];
  unsigned size;
  double *fw, *dout;

};


#endif //LSP_CORR_H
