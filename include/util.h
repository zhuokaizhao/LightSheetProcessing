//! \file util.h
//! \author Jake Stover
//! \date 2018-04-10
//! \brief rewrite by Jiawei Jiang at 2018-06-25

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


//! \brief Throw LSPException if status is true,
void nrrd_checker(bool status, AirArray* mop, std::string prompt,
                 std::string file, std::string function);

//! \brief New an nrrd object with with smart free.
Nrrd* safe_nrrd_new(AirArray* mop);

//! \brief Load nrrd with error detection and smart free.
Nrrd* safe_load_nrrd(AirArray* mop, std::string filename);

#endif //LSP_UTIL_H
