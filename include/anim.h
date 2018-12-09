//! \file anim.h
//! \author Jake Stover
//! \date 04-24-2018
//! \brief Build small video based on projection files.
//! \brief rewrite by Jiawei Jiang at 07-01-2018

#ifndef LSP_AMIN_H
#define LSP_AMIN_H

#include <vector>

#include "CLI11.hpp"

using namespace std;

struct animOptions {
    std::string nhdr_path = "nhdr/";
    std::string proj_path = "proj/";
    std::string anim_path = "anim/";
    // restrict the number of files that we processed
    std::string maxFileNum;
    // base name used for nhdr, proj and potentially anim
    std::string base_name;
    // fps of the .avi output video
    int fps = 10;
    vector<int> allFileSerialNumber;
    uint tmax;
    uint dwn_sample = 2;    // How much to down-sample
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

    //! \brief calculate frame origins; return 1 if all origins is 0 or calculation fails.
    int set_origins();
    std::vector<std::vector<int>> origins;
    std::vector<std::vector<int>> minmax;

    animOptions const opt;
    airArray* mop; //in parallelized part, use a thread_loacal mop instead
};


#endif //LSP_AMIN_H
