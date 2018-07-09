//! \file corrfind.h
//! \author Jake Stover
//! \date 2018-05-09
//! \brief Find input nrrd/png file and compute corr for it.
//! \brief rewrite by Jiawei Jiang at 2018-06-26

#ifndef LSP_CORRFIND_H
#define LSP_CORRFIND_H

#include "CLI11.hpp"


struct corrfindOptions {
    std::string file_dir = "reg/";
    int file_number;
    std::string output_name = "-corr1.txt";
    std::vector<std::string> kernels = {"c4hexic", "c4hexicd"};
    unsigned int bound = 10;
    double epsilon = 0.00000000000001;
};

void setup_corrfind(CLI::App &app);

class Corrfind{
public:
	Corrfind(corrfindOptions const &opt = corrfindOptions());
	~Corrfind();

	void main();

private:
	corrfindOptions const opt;
	airArray* mop;
};

#endif //LSP_CORRFIND_H
