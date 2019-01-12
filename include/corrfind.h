//! \file corrfind.h
//! \author Jake Stover
//! \date 2018-05-09
//! \brief Find input nrrd/png file and compute corr for it.
//! \brief rewrite by Jiawei Jiang at 2018-06-26

#ifndef LSP_CORRFIND_H
#define LSP_CORRFIND_H

#include "CLI11.hpp"


struct corrfindOptions 
{
    // input path that includes all images i-{XY, XZ, YZ}.png
    std::string image_path;
    // output path that saves the optimal alignment
    std::string align_path;
    std::string output_file;
    std::vector<std::string> input_images;
    vector< vector< pair<int, string> > > inputImages;
    int file_number;
    std::vector<std::string> kernel = {"c4hexic", "c4hexicd"};
    unsigned int bound = 20;
    double epsilon = 0.00000000000001;
    int verbose = 0;
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
