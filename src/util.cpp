//
// Created by Jake Stover on 4/10/18.
//

#include "util.h"

#include <iostream>
#include <iterator>
#include <algorithm>
#include <teem/nrrd.h>

/*
 * CLASSES
 */

// LSPException Class
LSPException::LSPException(const char *_msg, const char *_file, const char *_func) : std::runtime_error(_msg),
                                                                                     file(_file),
                                                                                     func(_func) {}
LSPException::LSPException(const std::string _msg, const char *_file, const char *_func) : std::runtime_error(_msg.c_str()),
                                                                                     file(_file),
                                                                                     func(_func) {}
const char* LSPException::get_file() const { return file; }
const char* LSPException::get_func() const { return func; }


/*
 * UTILITY FUNCTIONS
 */

std::string zero_pad(int num, uint len) {
    std::string ret = std::to_string(num);
    ret = std::string(len-ret.size(), '0') + ret;

    return ret;
}


template<typename T>
std::ostream &operator<<(std::ostream &os, std::vector<T> vec){
  std::copy(vec.begin(), vec.end(), std::ostream_iterator<T>(os, " "));
  return os;
}
template std::ostream &operator<<(std::ostream &os, std::vector<double> vec);
// Ugly trick here: W/o this call, lib will not build specialization for double type,
// then compiler cannot link files correctly.


void nrrd_checker(bool status, airArray* mop, std::string prompt,
                 std::string file, std::string function){
  if(status){
    char *err = biffGetDone(NRRD);
    std::string msg = prompt + std::string(err); 

    airMopAdd(mop, err, airFree, airMopAlways);
    airMopError(mop);

    throw LSPException(msg, file.c_str(), function.c_str());
  }
}


// REMEMBER: (nrrdNew-nrrdNuke) and (nrrdWrap-nrrdNix)
Nrrd* safe_nrrd_new(airArray* mop, airMopper mopper){
  Nrrd* nrrd = nrrdNew();
  airMopAdd(mop, nrrd, mopper, airMopAlways);

  return nrrd;
}


Nrrd* safe_nrrd_load(airArray* mop, std::string filename) {
  Nrrd *nin;

  /* create a nrrd; at this point this is just an empty container */
  nin = safe_nrrd_new(mop, (airMopper)nrrdNuke);

  /* read in the nrrd from file */
  nrrd_checker(nrrdLoad(nin, filename.c_str(), NULL),
              mop, "Error loading file: ", "util.cpp", "safe_nrrd_load");

  return nin;
}
