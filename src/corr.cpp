//
// Created by Gordone Kindlmann.
//
#include <math.h>

#include <teem/air.h>
#include <teem/biff.h>
#include <teem/nrrd.h>
#include <teem/limn.h>

#include "corr.h"
#include "util.h"
#include "skimczi.h"

#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

#include <chrono> 
#include <algorithm>

/* bb[0,0] is located at same position as aa[offx, offy] */

/*
Nrrd *ndebug = NULL;
float *bug;
unsigned int szbugx;
*/

struct myclass {
  bool operator() (int i,int j) { return (i<j);}
} corrSmallToLarge;

using namespace std;
namespace fs = boost::filesystem;

static double crossCorr(const unsigned short *aa, const unsigned short *bb,
                        const unsigned int sza[2], const unsigned int szb[2],
                        const int off[2]) 
{
    /* static const char me[]="crossCorr"; */
    int xi, yi, lo[2], hi[2];
    unsigned int ii;
    double dot /* numerator */, lena, lenb; /* factors in denominator */

    for (ii=0; ii<2; ii++) 
    {
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
    dot = lena = lenb = 0;
    /*
    nrrdAlloc_va(ndebug, nrrdTypeFloat, 3,
                AIR_CAST(size_t, 2),
                AIR_CAST(size_t, hi[0]-lo[0]+1),
                AIR_CAST(size_t, hi[1]-lo[1]+1));
    bug = AIR_CAST(float *, ndebug->data);
    szbugx = hi[0]-lo[0]+1;
    */
    int off0 = off[0];
    int off1 = off[1];
    int lo0 = lo[0];
    int lo1 = lo[1];
    int hi0 = hi[0];
    int hi1 = hi[1];
    unsigned int szb0 = szb[0];
    unsigned int sza0 = sza[0];
    
    for (yi=lo1; yi<=hi1; yi++) 
    {
        for (xi=lo0; xi<=hi0; xi++) 
        {
            float a, b;
            b = bb[xi + szb0*yi];
            a = aa[(xi+off0) + sza0*(yi+off1)];
            dot += a*b;
            lena += a*a;
            lenb += b*b;
        }
    }
    
    return dot/(sqrt(lena)*sqrt(lenb));
}

static int crossCorrImg(Nrrd *nout, int maxIdx[2], Nrrd *nin[2],
                        int bound, int verbose,
                        airArray *mop /* passing the mop just for convenience; more
                                        correct would be to use a new biff key
                                        for this function, but that's probably
                                        more confusing */) 
{
    static const char me[] = "crossCorrImg";
    char *err, done[13];
    unsigned int sza[2], szb[2], ii, szc;
    unsigned short *aa, *bb;
    double *cci, maxcc, cc;
    int ox, oy;

    for (ii=0; ii<2; ii++) 
    {
        if (!( 2 == nin[ii]->dim && nrrdTypeUShort == nin[ii]->type )) 
        {
            fprintf(stderr, "%s: input %s isn't 2D ushort array "
                            "(instead got %u-D %s array)\n", me, !ii ? "A" : "B",
                    nin[ii]->dim, airEnumStr(nrrdType, nin[ii]->type));
            return 1;
        }
    }
    sza[0] = nin[0]->axis[0].size;
    sza[1] = nin[0]->axis[1].size;
    szb[0] = nin[1]->axis[0].size;
    szb[1] = nin[1]->axis[1].size;
    aa = AIR_CAST(unsigned short *, nin[0]->data);
    bb = AIR_CAST(unsigned short *, nin[1]->data);

    szc = 2*bound + 1;
    if (nrrdAlloc_va(nout, nrrdTypeDouble, 2,
                    AIR_CAST(size_t, szc),
                    AIR_CAST(size_t, szc))) 
    {
        airMopAdd(mop, err = biffGetDone(NRRD), airFree, airMopAlways);
        fprintf(stderr, "%s: trouble allocating output:\n%s\n", me, err);
        return 1;
    }

    cci = AIR_CAST(double *, nout->data);

    maxcc = AIR_NEG_INF;
    if (verbose) 
    {
        fprintf(stderr, "%s: computing ...       ", me); fflush(stdout);
    }
    
    for (oy=-bound; oy<=bound; oy++) 
    {
        int off[2];
        if (verbose) 
        {
            fprintf(stderr, "%s", airDoneStr(-bound, oy, bound, done)); fflush(stdout);
        }
        
        off[1] = oy;
        for (ox=-bound; ox<=bound; ox++) 
        {
            off[0] = ox;
            cc = cci[ox+bound + szc*(oy+bound)] = crossCorr(aa, bb, sza, szb, off);
            /* remember where the max is */
            if (cc > maxcc) 
            {
                maxIdx[0] = ox;
                maxIdx[1] = oy;
                maxcc = cc;
            }
        }
    }
    if (verbose) 
    {
        fprintf(stderr, "%s\n", airDoneStr(-bound, oy, bound, done)); fflush(stdout);
    }

    return 0;
}

/* convolution based recon of value and derivative, with
   simplifying assumption that world == index space,
   and this is a square size-by-size data */
static double probe(double grad[2], /* output */
                    int *out, /* went outside domain */
                    const double *val, unsigned int size, const double pos[2],
                    double *fw, const NrrdKernelSpec *kk, const NrrdKernelSpec *dk,
                    int ksup, int ilo, int ihi) 
{
    /* static const char me[]="probe"; */
    int ii, jj, xn, yn, out0, out1, xi, yi;
    double res, xa, ya;

    double *fwD0x = fw;
    double *fwD0y = fw + 1*2*ksup;
    double *fwD1x = fw + 2*2*ksup;
    double *fwD1y = fw + 3*2*ksup;

    xn = floor(pos[0]);
    yn = floor(pos[1]);
    xa = pos[0] - xn;
    ya = pos[1] - yn;
    out0 = out1 = 0;
    
    for (ii=ilo; ii<=ihi; ii++) 
    {
        xi = xn + ii;
        if (xi < 0 || size-1 < xi) 
        {
            out0 += 1;
        }
        
        yi = yn + ii;
        
        if (yi < 0 || size-1 < yi) 
        {
            out1 += 1;
        }

        fwD0x[ii-ilo] = kk->kernel->eval1_d(xa-ii, kk->parm);
        fwD0y[ii-ilo] = kk->kernel->eval1_d(ya-ii, kk->parm);
        fwD1x[ii-ilo] = dk->kernel->eval1_d(xa-ii, dk->parm);
        fwD1y[ii-ilo] = dk->kernel->eval1_d(ya-ii, dk->parm);
        /*
        printf("fwD0x[%u] = %g\t", ii-ilo, fwD0x[ii-ilo]);
        printf("fwD0y[%u] = %g\t", ii-ilo, fwD0y[ii-ilo]);
        printf("fwD1x[%u] = %g\t", ii-ilo, fwD1x[ii-ilo]);
        printf("fwD1y[%u] = %g\n", ii-ilo, fwD1y[ii-ilo]);
        */
    }
    *out = out0 + out1;
    
    if (*out) 
    {
        grad[0] = AIR_NAN;
        grad[1] = AIR_NAN;
        return AIR_NAN;
    }
    /*
    printf("!%s: (%g,%g) = n(%u,%u) + a(%g,%g)\n", me,
            pos[0], pos[1], xn, yn, xa, ya);
    */
    res = grad[0] = grad[1] = 0;
    for (jj=ilo; jj<=ihi; jj++) 
    {
        yi = yn + jj;
        for (ii=ilo; ii<=ihi; ii++) 
        {
            xi = xn + ii;
            double vv = val[xi + size*yi];
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

void setup_corr(CLI::App &app) 
{
    auto opt = std::make_shared<corrOptions>();
    auto sub = app.add_subcommand("corr", "Simple program for measuring 2D image correlation"
                                            "over a 2D array of offsets. "
                                            "Prints out offset coordinates that maximized the "
                                            "cross correlation");

    //sub->add_option("-i, --ab", opt->input_images, "Two input images A and B to correlate.")->expected(2)->required();
    sub->add_option("-i, --input_path", opt->input_path, "Input path that includes images to be processed")->required();
    sub->add_option("-o, --output", opt->output_path, "Output file path")->required();
    sub->add_option("-b, --max", opt->max_offset, "Maximum offset (Default: 10).");
    sub->add_option("-k, --kdk", opt->kernel, "Kernel and derivative for resampleing cc output, or box box to skip this step. (Default: box box)");
    sub->add_option("-e, --epsilon", opt->epsilon, "Convergence for sub-resolution optimization. (Default: 0.0001)");
    sub->add_option("-v, --verbosity", opt->verbosity, "Verbosity level. (Default: 1)");
    sub->add_option("-m, --itermax", opt->max_iters, "Maximum number of iterations. (Default: 100)");

    sub->set_callback([opt]() 
    {
        try 
        {
            // check if input_path is valid, notice that there is no Single file mode for this task, has to be directory
            if (checkIfDirectory(opt->input_path))
            {
                cout << "Input path " << opt->input_path << " is valid, start processing" << endl << endl;
            
                const vector<string> images = GetDirectoryFiles(opt->input_path);
                
                // allValidImageNames stores all the .png image file names
                vector<string> allValidImageNames;
                // allImageTypes stores different image types, ex, x-avg.png is avg type, x-max.png is max type
                vector<string> allImageTypes;
                // allImageSerialNumber stores the sequence number of an image, ex, 100-avg.png, sequence number is 100
                vector<int> allImageSerialNumber;
                
                // we want to know all the image files in this input_path, which includes all the types
                int numAllTypeImages = 0;
                
                // imagesByType contains the names of images that have the same type, like avg, or max
                vector<string> allNamesofCurType;
                vector< pair< string, vector<string> > > imageNamesByType;

                for (int i = 0; i < images.size(); i++)
                {
                    // get the current file
                    string curImageName = images[i];
                    
                    // check if input file is a .png file
                    int end = curImageName.rfind(".png");
                    
                    // if this is indeed an image
                    if (end && (end == curImageName.length() - 4))
                    {
                        if (opt->verbosity)
                            cout << "Current input file " + curImageName + " ends with .png, count this image" << endl;
                        
                        numAllTypeImages++;
                        allValidImageNames.push_back(curImageName);

                        // normally mid = 3 (at least for the above example)
                        int mid = curImageName.rfind("-");
                        int start = 0;
                        int type_length = end - (mid+1);
                        int sequence_length = mid - start;
                        
                        string curType = curImageName.substr(mid+1, type_length);
                        // cout << "CurType is " << curType << endl;
                        // determine if this curType already in imageNamesByType
                        // if this type has not appeared
                        if (find(allImageTypes.begin(), allImageTypes.end(), curType) != allImageTypes.end())
                        {
                            // push this into the allImageTypes
                            allImageTypes.push_back(curImageName);

                            // now we initialize a vector to store all the names of this type
                            allNamesofCurType.push_back(curImageName);
                            // make the pair
                            pair < string, vector<string> > curPair = make_pair(curType, allNamesofCurType);
                            // push the pair into imageNamesByType
                            imageNamesByType.push_back(curPair);
                        }
                        // if this type already exists 
                        else
                        {
                            // iterate all the pairs
                            for (int i = 0; i < imageNamesByType.size(); i++)
                            {
                                // if they match
                                if (imageNamesByType[i].first == curType)
                                {
                                    imageNamesByType[i].second.push_back(curImageName);
                                }
                            }
                        }

                        // store the sequence number 
                        string sequenceNumString = curImageName.substr(start, sequence_length);
                        //cout << "CurNumber is " << sequenceNumString << endl;
                        
                        if (is_number(sequenceNumString))
                        {
                            int sequenceNum = stoi(sequenceNumString);
                            // we might have same sequence number but different types of images
                            // like 100-max.png and 100-avg.png, both have sequence number 100
                            if (find(allImageSerialNumber.begin(), allImageSerialNumber.end(), sequenceNum) != allImageSerialNumber.end())
                            {
                                allImageSerialNumber.push_back(sequenceNum);
                            }

                        }
                        else
                            cout << sequenceNumString << " is NOT a number" << endl;
                    }
                    // if this file is not an image, do nothing
                    else
                    {

                    }
                }

                // sort the image serial numbers
                sort(allImageSerialNumber.begin(), allImageSerialNumber.end(), corrSmallToLarge);

                for (int i = 0; i < allImageSerialNumber.size(); i++)
                    cout << allImageSerialNumber[i] << endl;
                
                // sanity checks
                if (allImageTypes.size() != imageNamesByType.size())
                    cout << endl << "Warnign: Something wrong with the image types processing" << endl;


                for (int i = 0; i < imageNamesByType.size(); i++)
                {
                    if (imageNamesByType[i].second.size() != allImageSerialNumber.size())
                        cout << endl << "Warnings: Something wrong with the image serial number" << endl;
                }

                // Process the images by pair can call corr_main
                // for each TYPE of images, we want to to correlation between i and i+1, i starts with 1
                for (int i = 0; i < imageNamesByType.size(); i++)
                {
                    // each pair has struct pair< string, vector<string> >
                    pair<string, vector<string> > curPair = imageNamesByType[i];
                    for (int j = 0; j < allImageSerialNumber.size(); j++)
                    {
                        // we have image pairs until j = length - 2
                        if (j < allImageSerialNumber.size()-1)
                        {
                            string input_image_1 = opt->input_path + to_string(allImageSerialNumber[j]) + "-" + imageNamesByType[i].first + ".png";
                            string input_image_2 = opt->input_path + to_string(allImageSerialNumber[j+1]) + "-" + imageNamesByType[i].first + ".png";
                            // double check that these two names exist in curPair
                            if (find(curPair.second.begin(), curPair.second.end(), input_image_1) != curPair.second.end()
                                    && find(curPair.second.begin(), curPair.second.end(), input_image_2) != curPair.second.end())
                            {
                                opt->input_images = {input_image_1, input_image_2};
                            }
                            else
                            {
                                cout << "Warning: generated sorted input image names does not exist" << endl;
                                return;
                            }

                            // put these output names in opt
                            opt->output_file = opt->output_path + to_string(allImageSerialNumber[j]) + "-" + imageNamesByType[i].first + "-corred";
                        }

                        // create output directory if not exist
                        if (!checkIfDirectory(opt->output_path))
                        {
                            boost::filesystem::create_directory(opt->output_path);
                            cout << "Output path " << opt->output_path << " does not exits, but has been created" << endl;
                        }

                        // then we can run corr_main
                        corr_main(*opt);
                    }
                }
            }
            else
            {
                cout << "Input path is invalid, program exits" << endl;
            }
            
        } 
        catch(LSPException &e) 
        {
            std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
        }
    });
}

std::vector<double> corr_main(corrOptions const &opt) 
{
    airArray *mop = airMopNew();

    const char *outName = opt.output_file.c_str();

    Nrrd *nin[2], *nout;
    int bound = opt.max_offset,
        maxIdx[2]={-1,-1},
        verbose = opt.verbosity,
        iterMax = opt.max_iters;
        NrrdKernelSpec *kk[2];

    double eps = opt.epsilon;

    std::vector<double> shift;


    nin[0] = safe_nrrd_load(mop, opt.input_images[0]);
    nin[1] = safe_nrrd_load(mop, opt.input_images[1]);

    kk[0] = nrrdKernelSpecNew();
    kk[1] = nrrdKernelSpecNew();
    nrrdKernelParse(&(kk[0]->kernel), kk[0]->parm, opt.kernel[0].c_str());
    nrrdKernelParse(&(kk[1]->kernel), kk[1]->parm, opt.kernel[1].c_str());

    nout = nrrdNew();
    airMopAdd(mop, nout, (airMopper)nrrdNuke, airMopAlways);
    airMopAdd(mop, nin[0], (airMopper)nrrdNuke, airMopAlways);
    airMopAdd(mop, nin[1], (airMopper)nrrdNuke, airMopAlways);
    airMopAdd(mop, kk[0], (airMopper)nrrdKernelSpecNix, airMopAlways);
    airMopAdd(mop, kk[1], (airMopper)nrrdKernelSpecNix, airMopAlways);

    if (crossCorrImg(nout, maxIdx, nin, bound, verbose, mop)) 
    {
        char *msg;
        char *err = biffGetDone(NRRD);

        sprintf(msg, "Error computing cross correlation: %s", err);

        airMopAdd(mop, err, airFree, airMopAlways);
        airMopError(mop);

        throw LSPException(msg, "corr.cpp", "corr_main");
    }

    if (nrrdKernelBox == kk[0]->kernel && nrrdKernelBox == kk[1]->kernel) 
    {
        if (AIR_ABS(maxIdx[0]) == bound || AIR_ABS(maxIdx[1]) == bound) 
        {
            char *msg;
            sprintf(msg, "maxIdx %d,%d is at boundary of test space; "
                        "should increase -b bound %d\n",
                    maxIdx[0], maxIdx[1], bound);

            airMopError(mop);

            throw LSPException(msg, "corr.cpp", "corr_main");
        }
        
        if(verbose)
            printf("%d %d = shift\n", maxIdx[0], maxIdx[1]);
    } 
    else 
    {
        double ksup0 = kk[0]->kernel->support(kk[0]->parm);
        double ksup1 = kk[1]->kernel->support(kk[1]->parm);
        double _ksup = AIR_MAX(ksup0, ksup1);
        int ksup = AIR_ROUNDUP(_ksup);
        int ilo = 1 - ksup;
        int ihi = ksup;
        double *fw = AIR_CALLOC(4*2*ksup, double);
        airMopAdd(mop, fw, airFree, airMopAlways);
        double *dout = AIR_CAST(double*, nout->data);
        unsigned int size = AIR_CAST(unsigned int, nout->axis[0].size);

        if (AIR_ABS(AIR_ABS(maxIdx[0]) - bound) + 1 <= ksup ||
            AIR_ABS(AIR_ABS(maxIdx[1]) - bound) + 1 <= ksup) 
        {
            char *msg;

            sprintf(msg, "maxIdx %d,%d is within kernel support %d "
                        "of test space boundary; should increase -b bound %d\n",
                    maxIdx[0], maxIdx[1], ksup, bound);

            airMopError(mop);

            throw LSPException(msg, "corr.cpp", "corr_main");
        }

        if (verbose) 
        {
            printf("%s->support=%g, %s->support=%g ==> ksup = %d\n",
                    kk[0]->kernel->name, ksup0,
                    kk[1]->kernel->name, ksup1, ksup);
        }
        int out, badstep, iter;
        double dval, val0, val1, grad0[2], grad1[2], pos0[2], pos1[2], hh=10, back=0.5, creep=1.2;
        pos0[0] = maxIdx[0] + bound;
        pos0[1] = maxIdx[1] + bound;

#define PROBE(V, G, P) \
    (V) = probe((G), &out, dout, size, (P), fw, kk[0], kk[1], ksup, ilo, ihi);

    PROBE(val0, grad0, pos0);
    
    if (verbose) 
    {
        printf("start: %g %g --> (out %d) %g (%g,%g)\n", pos0[0], pos0[0], out, val0, grad0[0], grad0[1]);
    }
    
    for (iter=0; iter<iterMax; iter++) 
    {
        int tries = 0;
        do 
        {
            ELL_2V_SCALE_ADD2(pos1, 1, pos0, hh, grad0);
            PROBE(val1, grad1, pos1);
            
            if (verbose > 1) 
            {
                printf("  try %d: %g %g --> (out %d) %g (%g,%g)\n", tries, pos1[0], pos1[0], out, val1, grad1[0], grad1[1]);
            }
            
            badstep = out || (val1 < val0);
            if (badstep) 
            {
                if (verbose > 1) 
                {
                    printf("  badstep!  %d || (%g < %g)\n", out, val1, val0);
                }
                hh *= back;
            } 
            else 
            {
                hh *= creep;
            }
            tries++;
        } while (badstep);
      
        dval = (val1 - val0)/val1;
        if (dval < eps) 
        {
            if (verbose) 
            {
                printf("converged in %d iters, %g < %g\n", iter, dval, eps);
            }
            break;
        }
        
        val0 = val1;
        ELL_2V_COPY(pos0, pos1);
        ELL_2V_COPY(grad0, grad1);
        
        if (verbose > 1) 
        {
            printf("%d: %f %f --> (%d tries) %f (%g,%g) (hh %g)\n", iter, pos0[0], pos0[0], tries, val0, grad0[0], grad0[1], hh);
        }
    }
    
    if(verbose)
        printf("%f %f = shift\n", pos1[0] - bound, pos1[1] - bound);
    
    shift.push_back(pos1[0]-bound);
    shift.push_back(pos1[1]-bound);
  }

    if (!opt.output_file.empty()) 
    {
        if (nrrdSave(outName, nout, NULL)) 
        {
            char *msg;
            char *err = biffGetDone(NRRD);

            sprintf(msg, "Error saving output: %s", err);

            airMopAdd(mop, err, airFree, airMopAlways);
            airMopError(mop);

            throw LSPException(msg, "corr.cpp", "corr_main");
        }
    }

    airMopOkay(mop);
    return shift;
}