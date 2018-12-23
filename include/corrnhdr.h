//! \file corrnhdr.h
//! \author Jake Stover
//! \date 2018-05-09
//! \brief Load corr of data, compute offsets and save new nhdr files.
//! \brief rewrite by Jiawei Jiang at 2018-06-25

#ifndef LSP_CORRNHDR_H
#define LSP_CORRNHDR_H

#include "CLI11.hpp"

struct corrnhdrOptions {
	// std::string file_dir = ".";
    std::string nhdr_path;
    std::string corr_path;
    std::string new_nhdr_path;
    // int num;
};

void setup_corrnhdr(CLI::App &app);

class Corrnhdr{
public:
	Corrnhdr(corrnhdrOptions const &opt = corrnhdrOptions());
	~Corrnhdr();

	void main();

private:
	void compute_offsets();
	void median_filtering();
	void smooth();

	corrnhdrOptions const opt;
	airArray* mop;
	// std::string file_dir;
    std::string nhdr_path, corr_path, new_nhdr_path;

	Nrrd *offset_origin, *offset_median, *offset_smooth;
};

#endif //LSP_CORRNHDR_H