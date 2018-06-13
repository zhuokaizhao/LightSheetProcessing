//
// Created by Jake Stover on 4/10/18.
//

#ifndef LSP_UTIL_H
#define LSP_UTIL_H

#include <teem/nrrd.h>
#include <string>

class LSPException : public std::runtime_error {
private:
    const char *file;
    const char *func;

public:
    LSPException(const char *_msg, const char *_file, const char *_func);
    LSPException(const std::string _msg, const char *_file, const char *_func);

    const char* get_file() const;
    const char* get_func() const;
};

std::string zero_pad(int num, unsigned int len);

Nrrd* safe_load_nrrd(std::string filename);

#endif //LSP_UTIL_H
