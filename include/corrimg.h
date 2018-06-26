//! \file corrimg.h
//! \author Jake Stover
//! \date 2018-05-09
//! \brief Convert(Resample) projection nrrds into 2D-pngs.
//! \brief rewrite by Jiawei Jiang at 2018-06-25

#ifndef LSP_CORRIMG_H
#define LSP_CORRIMG_H

#include "CLI11.hpp"

struct corrimgOptions {
    std::string input_file;
    std::string output_file;
    std::string kernel = "Gauss:10,4";
};

void setup_corrimg(CLI::App &app);

class Corrimg{
public:
	Corrimg(corrimgOptions const &opt = corrimgOptions());

	void main();

private:
	corrimgOptions const opt;
	Nrrd *nrrd1, *nrrd2;
	AirArray* mop;
};

#endif //LSP_CORRIMG_H
