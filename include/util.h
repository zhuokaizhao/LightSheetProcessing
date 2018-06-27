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

//! \brief Simple overriding of printing std::vector.
template<typename T>
std::ostream &operator<<(std::ostream &os, std::vector<T> vec);

//! \brief Throw LSPException if status is true.
void nrrd_checker(bool status, airArray* mop, std::string prompt,
                 std::string file, std::string function);

//! \brief New an nrrd object with with smart free.
Nrrd* safe_nrrd_new(airArray* mop, airMopper mopper);

//! \brief Load nrrd with error detection and smart free.
Nrrd* safe_nrrd_load(airArray* mop, std::string filename);

#endif //LSP_UTIL_H
