//! \file anim.h
//! \author Jake Stover
//! \date 04-24-2018
//! \brief Build small video based on projection files.
//! \brief rewrite by Jiawei Jiang at 07-01-2018

#ifndef LSP_AMIN_H
#define LSP_AMIN_H

#include "CLI11.hpp"

struct animOptions {
    std::string proj_path = "proj/";
    std::string anim_path = "anim/";
    uint tmax;
    uint dwn_sample = 1;    // How much to down-sample
    double scale_x = 1.0;
    double scale_z = 1.0;
    uint verbose = 0;
};

void setup_anim(CLI::App &app);

class Anim{
public:
	Anim(animOptions const &opt = animOptions());
	~Anim();

  void main();

private:
  	void split_type();
    void make_max_frame(std::string);
  	void make_avg_frame(std::string);
    void build_png();
  	void build_video();

	animOptions const opt;
	airArray* mop;
};


#endif //LSP_AMIN_H
