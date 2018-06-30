//
// Created by Jake Stover on 4/10/18.
//

#include "util.h"

#include <iostream>
#include <iterator>
#include <algorithm>
#include <teem/nrrd.h>
#include <libxml/parser.h>

/*
 * CLASSES
 */

// LSPException Class
LSPException::LSPException(std::string const &_msg, std::string const &_file, std::string const &_func) : std::runtime_error(_msg.c_str()),
                                                                                     file(_file),
                                                                                     func(_func) {}
std::string const & LSPException::get_file() const { return file; }
std::string const & LSPException::get_func() const { return func; }


/*
 * UTILITY FUNCTIONS
 */

void nrrd_checker(bool status, airArray* mop, std::string prompt,
                 std::string file, std::string function){
  if(status){
    char *err = biffGetDone(NRRD);
    std::string msg = prompt + std::string(err); 

    airMopAdd(mop, err, airFree, airMopAlways);

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
              mop, "Error loading file:\n", "util.cpp", "safe_nrrd_load");

  return nin;
}


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

Xml_getter::Xml_getter(std::string file)
: doc(xmlParseFile(file.c_str())),
  node(xmlDocGetRootElement(doc)) {}

Xml_getter::~Xml_getter(){
  xmlFreeDoc(doc);
  xmlCleanupParser();
}

std::string Xml_getter::operator()(std::string p){
  node = xmlDocGetRootElement(doc);
  pattern = p;
  search();
  return val;
}

void Xml_getter::search(){
  for(auto cur_node = node; cur_node; cur_node = cur_node->next){
    if(cur_node->type == XML_ELEMENT_NODE)
      if(!xmlStrcmp(cur_node->name, (const xmlChar*)pattern.c_str())){
        xmlChar* tmp_v = xmlNodeGetContent(cur_node);
        val = (char const*)tmp_v;
        xmlFree(tmp_v);
      }

    node = cur_node->children;
    search();
  }
}
