//
// Created by Jake Stover on 4/10/18.
//

#include "util.h"

#include <iostream>
#include <sstream>
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
  std::stringstream ss;

  // the number is converted to string with the help of stringstream
  ss << num;
  std::string ret;
  ss >> ret;

  // Append zero chars
  u_long str_length = ret.length();
  for (int i = 0; i < len - str_length; i++)
    ret.insert(0, "0");
  return ret;
}


Nrrd* safe_load_nrrd(std::string filename) {
  Nrrd *nin;

  /* create a nrrd; at this point this is just an empty container */
  nin = nrrdNew();

  /* read in the nrrd from file */
  if (nrrdLoad(nin, filename.c_str(), NULL)) {
    char *err = biffGetDone(NRRD);
    std::string msg = "Error loading file: ";
    msg.append(err);

    free(err);

    throw LSPException(msg, "util.cpp", "safe_load_nrrd");
  }

  return nin;
}