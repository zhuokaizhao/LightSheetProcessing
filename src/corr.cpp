//
// Created by Gordone Kindlmann.
//
#include <cmath>

#include <teem/air.h>
#include <teem/biff.h>
#include <teem/nrrd.h>
#include <teem/limn.h>

#include "corr.h"
#include "util.h"

void setup_corr(CLI::App &app) {
  auto opt = std::make_shared<corrOptions>();
  auto sub = app.add_subcommand("corr", "Simple program for measuring 2D image correlation"
                                        "over a 2D array of offsets. "
                                        "Prints out offset coordinates that maximized the "
                                        "cross correlation");

  sub->add_option("-i, --ab", opt->input_images, "Two input images A and B to correlate.")->expected(2)->required();
  sub->add_option("-b, --max", opt->max_offset, "Maximum offset (Default: 10).");
  sub->add_option("-k, --kdk", opt->kernel, "Kernel and derivative for resampleing cc output, or box box to skip this step. (Default: box box)")->expected(2);
  sub->add_option("-o, --output", opt->output_file, "Output filename");
  sub->add_option("-e, --epsilon", opt->epsilon, "Convergence for sub-resolution optimization. (Default: 0.0001)");
  sub->add_option("-v, --verbosity", opt->verbosity, "Verbosity level. (Default: 1)");
  sub->add_option("-m, --itermax", opt->max_iters, "Maximum number of iterations. (Default: 100)");

  sub->set_callback([opt]() {
      try {
        Corr(*opt).main();
      } catch(LSPException &e) {
        std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
      }
  });
}

Corr::Corr(corrOptions const &opt): opt(opt), mop(airMopNew()) {
  nin[0] = safe_nrrd_load(mop, opt.input_images[0]);
  nin[1] = safe_nrrd_load(mop, opt.input_images[1]);
}


Corr::~Corr(){
  airMopOkay(mop);
}

/* bb[0,0] is located at same position as aa[offx, offy] */

/*
Nrrd *ndebug = NULL;
float *bug;
unsigned int szbugx;
*/

double Corr::cross_corr(const unsigned short *aa, const unsigned short *bb,
          const unsigned int sza[2], const unsigned int szb[2],
          const int off[2]) {
  int lo[2], hi[2];
  for (auto ii=0; ii<2; ii++) {
    /*  different scenerios to make sure computation of
        lo and hi bounds of index into bb are correct
      aa:  0  1  2  3  4  5  6     (sizeA == 7)
      bb:           0  1  2  3  4  (sizeB == 5, off == 3)

      aa:  0  1  2  3  4  5  6     (sizeA == 7)
      bb:           0  1  2        (sizeB == 3, off == 3)

      aa:       0  1  2  3  4  5  6  (sizeA == 7)
      bb: 0  1  2  3  4              (sizeB == 5, off == -2)

      aa:       0  1  2  3           (sizeA == 4)
      bb: 0  1  2  3  4  5  6        (sizeB == 7, off == -2)
    */
    lo[ii] = AIR_MAX(0, -off[ii]);
    hi[ii] = AIR_MIN(sza[ii]-1-off[ii], szb[ii]-1); 
    /*
    printf("%s: lo[%d] = max(0, -off[%d]) = max(0, %d) = %d\n", me,
           ii, ii, -off[ii], lo[ii]);
    printf("%s: hi[%d] = min(sza[%d]-1-off[%d], szb[%d]-1) = "
           "min(%u - %d, %u) = %d\n", me,
           ii, ii, ii, ii, sza[ii]-1, off[ii], szb[ii]-1, hi[ii]);
    */
  }
  double dot /* numerator */,
         lena, lenb; /* factors in denominator */
  /*
  nrrdAlloc_va(ndebug, nrrdTypeFloat, 3,
               AIR_CAST(size_t, 2),
               AIR_CAST(size_t, hi[0]-lo[0]+1),
               AIR_CAST(size_t, hi[1]-lo[1]+1));
  bug = AIR_CAST(float *, ndebug->data);
  szbugx = hi[0]-lo[0]+1;
  */
  for (auto yi=lo[1]; yi<=hi[1]; yi++) {
    for (auto xi=lo[0]; xi<=hi[0]; xi++) {
      double a = aa[(xi+off[0]) + sza[0]*(yi+off[1])];
      double b = bb[xi + szb[0]*yi];
      dot += a*b;
      lena += a*a;
      lenb += b*b;
    }
  }
  return dot/(sqrt(lena)*sqrt(lenb));
}


void Corr::cross_corrImg() {
  int bound = opt.max_offset,
      verbose = opt.verbosity;

  for (auto ii: {0, 1}) {
    if (!( 2 == nin[ii]->dim && nrrdTypeUShort == nin[ii]->type )) {
      std::string msg = "Error: input " + std::string(!ii?"A":"B") + "isn't 2D ushort array (instead got "
                        + std::to_string(nin[ii]->dim) + "-D " + airEnumStr(nrrdType, nin[ii]->type) + " array)";
      throw LSPException(msg, "corr.cpp", "Corr::cross_corrImg");
    }
  }
  unsigned int sza[2] = {AIR_CAST(unsigned int, nin[0]->axis[0].size),
                         AIR_CAST(unsigned int, nin[0]->axis[1].size)};
  unsigned int szb[2] = {AIR_CAST(unsigned int, nin[1]->axis[0].size),
                         AIR_CAST(unsigned int, nin[1]->axis[1].size)};
  auto aa = AIR_CAST(unsigned short *, nin[0]->data);
  auto bb = AIR_CAST(unsigned short *, nin[1]->data);

  auto szc = 2*bound + 1;
  nout = safe_nrrd_new(mop, (airMopper)nrrdNix);
  nrrd_checker(nrrdAlloc_va(nout, nrrdTypeDouble, 2,
                   AIR_CAST(size_t, szc),
                   AIR_CAST(size_t, szc)),
              mop, "Error allocating output: ", "corr.cpp", "Corr::cross_corrImg");

  auto cci = AIR_CAST(double *, nout->data);

  if (verbose)
    std::cerr << "Corr::cross_corrImg: computing ...      " << std::endl;

  double maxcc = AIR_NEG_INF;
  for (auto oy=-bound; oy<=bound; oy++) {
    if (verbose){
      char done[13]; // defined in teem.
      std::cerr << airDoneStr(-bound, oy, bound, done) << std::endl;
    }

    int off[2];
    off[1] = oy;
    for (auto ox=-bound; ox<=bound; ox++) {
      off[0] = ox;
      auto cc = cci[ox+bound + szc*(oy+bound)] = cross_corr(aa, bb, sza, szb, off);
      /* remember where the max is */
      if (cc > maxcc) {
        maxIdx[0] = ox;
        maxIdx[1] = oy;
        maxcc = cc;
      }
    }
  }
  if (verbose){
    char done[13]; // defined in teem.
    std::cerr << airDoneStr(-bound, bound+1, bound, done) << std::endl;
  }

}


void Corr::set_kernel(){
  int bound = opt.max_offset;

  kk[0] = nrrdKernelSpecNew();
  kk[1] = nrrdKernelSpecNew();
  nrrdKernelParse(&(kk[0]->kernel), kk[0]->parm, opt.kernel[0].c_str());
  nrrdKernelParse(&(kk[1]->kernel), kk[1]->parm, opt.kernel[1].c_str());
  airMopAdd(mop, kk[0], (airMopper)nrrdKernelSpecNix, airMopAlways);
  airMopAdd(mop, kk[1], (airMopper)nrrdKernelSpecNix, airMopAlways);

  if (nrrdKernelBox == kk[0]->kernel && nrrdKernelBox == kk[1]->kernel) {
    std::string msg = "maxIdx " + std::to_string(maxIdx[0]) + ", " + std::to_string(maxIdx[1])
                      + " is at boundary of test space; should increase -b bound " + std::to_string(bound) + "\n";
    if(AIR_ABS(maxIdx[0]) == bound || AIR_ABS(maxIdx[1]) == bound)
      throw LSPException(msg, "corr.cpp", "Corr::main");

    printf("%d %d = shift\n", maxIdx[0], maxIdx[1]);
  }
  else {
    double ksup0 = kk[0]->kernel->support(kk[0]->parm);
    double ksup1 = kk[1]->kernel->support(kk[1]->parm);
    ksup = AIR_ROUNDUP( AIR_MAX(ksup0, ksup1) );
    ilo = 1 - ksup;
    ihi = ksup;

    fw = AIR_CALLOC(4*2*ksup, double);
    airMopAdd(mop, fw, airFree, airMopAlways);
    dout = AIR_CAST(double*, nout->data);
    size = AIR_CAST(unsigned int, nout->axis[0].size);

    std::string msg = "maxIdx " + std::to_string(maxIdx[0]) + ", " + std::to_string(maxIdx[1])
                      + " is within kernel support " + std::to_string(ksup)
                      + " of test space boundary; should increase -b bound " + std::to_string(bound) + "\n";
      if(AIR_ABS(AIR_ABS(maxIdx[0]) - bound) + 1 <= ksup ||
                    AIR_ABS(AIR_ABS(maxIdx[1]) - bound) + 1 <= ksup)
        throw LSPException(msg, "corr.cpp", "Corr::main");

    if (opt.verbosity) {
      printf("%s->support=%g, %s->support=%g ==> ksup = %d\n",
             kk[0]->kernel->name, ksup0,
             kk[1]->kernel->name, ksup1, ksup);
    }
  }
}


/* convolution based recon of value and derivative, with
   simplifying assumption that world == index space,
   and this is a square size-by-size data */
double Corr::probe(double grad[2], /* output */
      const double pos[2],
      int *out /* went outside domain */) {

  double *fwD0x = fw;
  double *fwD0y = fw + 1*2*ksup;
  double *fwD1x = fw + 2*2*ksup;
  double *fwD1y = fw + 3*2*ksup;

  int xn = floor(pos[0]), yn = floor(pos[1]);
  double xa = pos[0] - xn, ya = pos[1] - yn;

  int out0 = 0, out1 = 0;
  for (auto ii=ilo; ii<=ihi; ii++) {
    auto xi = xn + ii;
    if (xi < 0 || size-1 < xi)
      out0 += 1;

    auto yi = yn + ii;
    if (yi < 0 || size-1 < yi)
      out1 += 1;

    fwD0x[ii-ilo] = kk[0]->kernel->eval1_d(xa-ii, kk[0]->parm);
    fwD0y[ii-ilo] = kk[0]->kernel->eval1_d(ya-ii, kk[0]->parm);
    fwD1x[ii-ilo] = kk[1]->kernel->eval1_d(xa-ii, kk[1]->parm);
    fwD1y[ii-ilo] = kk[1]->kernel->eval1_d(ya-ii, kk[1]->parm);
    /*
    printf("in loop: %d, %d, %d\n", ii, ilo, ihi);
    printf("fwD0x[%u] = %g\t", ii-ilo, fwD0x[ii-ilo]);
    printf("fwD0y[%u] = %g\t", ii-ilo, fwD0y[ii-ilo]);
    printf("fwD1x[%u] = %g\t", ii-ilo, fwD1x[ii-ilo]);
    printf("fwD1y[%u] = %g\n", ii-ilo, fwD1y[ii-ilo]);
    */
  }

  *out = out0 + out1;
  if (*out) {
    grad[0] = AIR_NAN;
    grad[1] = AIR_NAN;
    return AIR_NAN;
  }
  /*
  printf("!%s: (%g,%g) = n(%u,%u) + a(%g,%g)\n", me,
         pos[0], pos[1], xn, yn, xa, ya);
  */
  double res = grad[0] = grad[1] = 0;
  for (auto jj=ilo; jj<=ihi; jj++) {
    auto yi = yn + jj;
    for (auto ii=ilo; ii<=ihi; ii++) {
      auto xi = xn + ii;
      double vv = dout[xi + size*yi];
      /*
      printf("!%s: [%d,%d] : data.[%u + %u*%u = %u] = %g\n", me,
             ii, jj, xi, size, yi, xi + size*yi, vv);
      */
      res += vv*fwD0x[ii-ilo]*fwD0y[jj-ilo];
      grad[0] += vv*fwD1x[ii-ilo]*fwD0y[jj-ilo];
      grad[1] += vv*fwD0x[ii-ilo]*fwD1y[jj-ilo];
      /*
      printf("%g=res <==  vv*fwD0x[%d]*fwD0y[%d] = %g * %g * %g = %g\n",
             res, ii-ilo, jj-ilo, vv, fwD0x[ii-ilo], fwD0y[jj-ilo],
             vv*fwD0x[ii-ilo]*fwD0y[jj-ilo]);
      printf("%g=grad[0] <==  vv*fwD0x[%d]*fwD0y[%d] = %g * %g * %g = %g\n",
             grad[0], ii-ilo, jj-ilo, vv, fwD1x[ii-ilo], fwD0y[jj-ilo],
             vv*fwD1x[ii-ilo]*fwD0y[jj-ilo]);
      */
    }
  }
  return res;
}
void Corr::compute_shift(){
  int bound = opt.max_offset,
      verbose = opt.verbosity,
      iterMax = opt.max_iters;
  double eps = opt.epsilon;

  int out = 0;
  double grad0[2], grad1[2], pos0[2], pos1[2], hh=10, back=0.5, creep=1.2;
  double val0, val1;

  pos0[0] = maxIdx[0] + bound;
  pos0[1] = maxIdx[1] + bound;
  val0 = probe(grad0, pos0, &out);

  if (verbose) {
    printf("start: %g %g --> (out %d) %g (%g,%g)\n", pos0[0], pos0[0], out, val0, grad0[0], grad0[1]);
  }
  for (auto iter=0; iter<iterMax; iter++) {
    int tries, badstep;
    do {
      ELL_2V_SCALE_ADD2(pos1, 1, pos0, hh, grad0);
      val1 = probe(grad1, pos1, &out);
      if (verbose > 1) {
        printf("  try %d: %g %g --> (out %d) %g (%g,%g)\n", tries, pos1[0], pos1[0], out, val1, grad1[0], grad1[1]);
      }
      badstep = out || (val1 < val0);
      if (badstep) {
        if (verbose > 1) {
          printf("  badstep!  %d || (%g < %g)\n", out, val1, val0);
        }
        hh *= back;
      } else {
        hh *= creep;
      }
      tries++;
    } while (badstep);
    double dval = (val1 - val0)/val1;
    if (dval < eps) {
      if (verbose) {
        printf("converged in %d iters, %g < %g\n", iter, dval, eps);
      }
      break;
    }
    val0 = val1;
    ELL_2V_COPY(pos0, pos1);
    ELL_2V_COPY(grad0, grad1);
    if (verbose > 1) {
      printf("%d: %f %f --> (%d tries) %f (%g,%g) (hh %g)\n", iter, pos0[0], pos0[0], tries, val0, grad0[0], grad0[1], hh);
    }
  }
  printf("%f %f = shift\n", pos1[0] - bound, pos1[1] - bound);
  shift.push_back(pos1[0]-bound);
  shift.push_back(pos1[1]-bound);
}


std::vector<double> Corr::get_shift(){
  return shift;
}


void Corr::main() {
  cross_corrImg();

  set_kernel();

  compute_shift();

  if (!opt.output_file.empty())
    nrrd_checker(nrrdSave(opt.output_file.c_str(), nout, NULL),
                mop, "Error saving output: ", "corr.cpp", "corr_main");
}