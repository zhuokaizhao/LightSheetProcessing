//
// Created by Jake Stover on 4/24/18.
//

#ifndef LSP_AMIN_H
#define LSP_AMIN_H

#include "CLI11.hpp"

struct AnimOptions {
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
	Anim(AnimOptions const &opt = AnimOptions());
	~Anim();

  void main();

private:
  	void split_type();
    void make_max_frame(std::vector<Nrrd*>);
  	void make_avg_frame(std::vector<Nrrd*>);
  	void assembling_frame();

	AnimOptions const opt;
	airArray* mop;

	std::vector<Nrrd*> max_x_frames, max_z_frames, avg_x_frames, avg_z_frames;
};


#endif //LSP_AMIN_H
