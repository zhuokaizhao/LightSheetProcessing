//! \file pack.h
//! \author Jiawei Jiang
//! \date 2018-06-27
//! \brief Driver for standard dataset format.

// A standard dataset is:
// 1. original data is in 'some_path/czi' folder.
// 2. data files are named: some_name.czi, some_name(1).czi, some_name(2).czi...
// 3. this driver will generate work folders under "some_path/" and output files in there.


#ifndef PACK_H
#define PACK_H

#include "CLI11.hpp"
#include <boost/filesystem.hpp>
#include "util.h"

struct packOptions{
	std::string data_dir;
	std::string command = "all";
};

void setup_pack(CLI::App &app);

class Pack{
public:
	Pack(packOptions const &opt);

	void main();

private:
	void run_skim();
	void run_anim();
	void run_nhdrcheck();
	void run_untext();
	void run_corrimg();
	void run_corrfind();
	void run_corrnhdr();
	void run_anim_corr();
	void run_all();

	int find_tmax();

	//! \breif Check if path exists, if not throw LSPException.
	boost::filesystem::path safe_path(std::string const &folder);

	packOptions const opt;
	std::string data_dir;
	int tmax = -1;
};


#endif
