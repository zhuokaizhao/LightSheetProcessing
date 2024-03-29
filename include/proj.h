//! \file proj.h
//! \author Jiawei Jiang
//! \date 07-01-2018
//!	brief Create projection files based on nhdr file.
// Modified by Zhuokai Zhao


#ifndef LSP_PROJ_H
#define LSP_PROJ_H

#include "CLI11.hpp"

struct projOptions {
	int file_number = 0;
	std::string nhdr_path;
    //std::string czi_path;
	std::string proj_path;
    // we no longer need base name
    //std::string base_name;
    std::string file_name;
    int number_of_processed = 0;
    int verbose = 0;
};

void setup_proj(CLI::App &app);

class Proj{
public:
	Proj(projOptions const &opt = projOptions());
	~Proj();

	void main();

private:
	std::string nhdr_name, proj_common;
	projOptions opt;
	airArray* mop;
};

#endif