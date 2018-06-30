//! \file util.h
//! \author Jake Stover
//! \date 2018-04-10
//! \brief rewrite by Jiawei Jiang at 2018-06-25

#ifndef LSP_UTIL_H
#define LSP_UTIL_H

#include <stdexcept>
#include <vector>
#include <string>
#include <teem/nrrd.h>
#include <libxml/parser.h>

//! \brief LSP exception class.
class LSPException : public std::runtime_error {
private:
    std::string const file;
    std::string const func;

public:
	//! \brief Deteceted error"msg" in function "func" of "file".
    LSPException(std::string const &_msg, std::string const &_file, std::string const &_func);

    std::string const &get_file() const;
    std::string const &get_func() const;
};

//! \brief Change num to string and add padding zeros before the number.
std::string zero_pad(int num, unsigned int len);

//! \brief Throw LSPException if status is true.
void nrrd_checker(bool status, airArray* mop, std::string prompt,
                 std::string file, std::string function);

//! \brief New an nrrd object with with smart free.
Nrrd* safe_nrrd_new(airArray* mop, airMopper mopper);

//! \brief Load nrrd with error detection and smart free.
Nrrd* safe_nrrd_load(airArray* mop, std::string filename);

//! \brief Simple overriding of printing std::vector.
template<typename T>
std::ostream &operator<<(std::ostream &os, std::vector<T> vec);

//! \brief Functor to find element value(double type) in xml file
class Xml_getter{
public:
  Xml_getter(std::string file);
  ~Xml_getter();

  std::string operator()(std::string p);

private:  
  void search();

  xmlDocPtr doc;
  xmlNodePtr node;
  std::string pattern;
  std::string val;
};


#endif //LSP_UTIL_H
