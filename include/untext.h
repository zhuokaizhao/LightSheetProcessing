//! \file untext.h
//! \author Jiawei Jiang
//! \date 07-11-2018
//! \brief Untexture a projection file.

#ifndef LSP_UNTEXT_H
#define LSP_UNTEXT_H

#include "CLI11.hpp"

#include <fftw3.h>
#include <complex>
#include <vector>

struct untextOptions {
    std::string input;
    std::string output;
};

void setup_untext(CLI::App &app);

class Untext{
public:
	Untext(untextOptions const &opt = untextOptions());
	~Untext();

	void main();
private:
	void masking();
	Nrrd* untext_slice(Nrrd* proj, int ch, int type);

	untextOptions const opt;
	airArray* mop;

	Nrrd* nin;
	size_t szx, szy;
	std::vector<std::complex<float>> ft;
	fftwf_plan p, ip;

};

#endif //LSP_UNTEXT_H
