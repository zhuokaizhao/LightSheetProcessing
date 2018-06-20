//! \file util.h
//! \author Jake Stover
//! \date 2018-04-10

#ifndef LSP_UTIL_H
#define LSP_UTIL_H

#include <stdexcept>
#include <teem/nrrd.h>
#include <string>

//! \brief LSP exception class.
class LSPException : public std::runtime_error {
private:
    const char *file;
    const char *func;

public:
	//! \brief Deteceted error"msg" in function "func" of "file".
    LSPException(const char *_msg, const char *_file, const char *_func);
    LSPException(const std::string _msg, const char *_file, const char *_func);

    const char* get_file() const;
    const char* get_func() const;
};

//! \brief Change num to string and add padding zeros before the number.
std::string zero_pad(int num, unsigned int len);

//! \brief Load nrrd with error detection.
Nrrd* safe_load_nrrd(std::string filename);

#endif //LSP_UTIL_H
