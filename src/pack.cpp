//
//Created by Jiawei Jiang at 06/27/2018
//

#include <boost/filesystem.hpp>
#include <regex>
#include <omp.h>

#include "CLI11.hpp"

#include "skimczi.h"
#include "anim.h"
#include "nhdrCheck.h"
#include "untext.h"
#include "corrimg.h"
#include "corrfind.h"
#include "corrnhdr.h"
#include "anim.h"
#include "proj.h"

#include "pack.h"

using namespace boost::filesystem;

void setup_pack(CLI::App &app){
	auto opt = std::make_shared<packOptions>();
	auto sub = app.add_subcommand("pack", "Process dataset with standard format");

	sub->add_option("directory", opt->data_dir, "Where the 'czi' folder is")->required();
	sub->add_option("-c, --command", opt->command, "Specify the command you want to run. (Default: all)");

	sub->set_callback([opt](){
		try{
			Pack(*opt).main();
		}
		catch(LSPException &e){
			std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
		}
	});
}


Pack::Pack(packOptions const &opt): opt(opt), data_dir(canonical(opt.data_dir).string()){
  //better change path to absolute and resolve links if neccessary.
  
  //set thread num
  omp_set_num_threads(16);
}


path Pack::safe_path(std::string const &folder){
  //check if path is vaild.
  if(!exists(folder))
    throw LSPException("Error finding path: "+ folder, "pack.cpp", "Pack::safe_path");

  return path(folder);
}


int Pack::find_tmax(){
	// already found
	if(tmax != -1)
		return tmax;

	std::string exist_path, path_pattern;
	if(exists(data_dir+"/nhdr/")){
		//find the max file number based on nhdr/ (for run_proj)
		exist_path = data_dir+"/nhdr/";
		//find "iii" in pattern "../../iii.nhdr"
		path_pattern = "(\\w{3})\\.nhdr";
	}
	else if(exists(data_dir+"/reg/")){
		//find the max file number based on reg/ (for corr*s)
		exist_path = data_dir+"/reg/";
		//find "iii" in pattern "../../iii-projXX.nrrd"
		path_pattern = "(\\w{3})-proj\\w{2}\\.png";
	}
	else
		throw LSPException("Error finding tmax: "
												"proj/ and reg/ not exist. ",
											"pack.cpp", "Pack::find_tmax");

	directory_iterator end_iter;
	for(directory_iterator iter(exist_path); iter!=end_iter; ++iter){
		if(is_regular_file(iter->path())){
			std::string current_file = iter->path().string();
		  std::regex pattern(path_pattern);
		  if(std::regex_search(current_file, pattern)){
		    std::smatch results;
		    std::regex_search(current_file, results, pattern);
		    int num = std::stoi(results[1]);
		    if(num > tmax)
		    	tmax = num;
  		}
  	}
  }
  return tmax;
}


void Pack::run_skim(){
	//build related folders
	//for(std::string str: {"/nhdr/", "/proj/", "/xml/"}){
	for(std::string str: {"/nhdr/", "/xml/"}){
		if(!exists(data_dir+str))
			create_directory(data_dir+str);
	}

	//loop all files
	directory_iterator end_iter;
	std::vector<path> paths;
	for(auto iter = directory_iterator(safe_path(data_dir+"/czi/")); iter!=end_iter; ++iter)
		paths.push_back(iter->path());
	#pragma omp parallel for
	for(auto i=0; i<paths.size(); ++i){
		if(is_regular_file(paths[i])){
			std::string current_file = paths[i].string();
			//find file number
			int num = 0;
			std::regex pattern("[(](.*)[)]");
			if(std::regex_search(current_file, pattern)){
			    std::smatch results;
			    std::regex_search(current_file, results, pattern);
			    num = std::stoi(results[1]);
	  		}

	  		std::string iii = zero_pad(num, 3);
	  		SkimOptions skim_opt;
	  		skim_opt.file = current_file;
	  		skim_opt.no	= data_dir + "/nhdr/" + iii + ".nhdr";
	  		skim_opt.xo = data_dir + "/xml/" + iii + ".xml";
	  		skim_opt.po = data_dir + "/proj/" + iii; //do not create projection here.
			Skim(skim_opt).main();
		}
	}
}


void Pack::run_proj(std::string nhdr, std::string proj){
	if(!exists(data_dir+proj))
		create_directory(data_dir+proj);

	tmax = find_tmax();
	#pragma omp parallel for
	for(auto i=0; i<=tmax; ++i){
		projOptions proj_opt;
		proj_opt.file_number = i;
		proj_opt.nhdr_path = data_dir + nhdr;
		proj_opt.proj_path = data_dir + proj;
		Proj(proj_opt).main();
	}
}


void Pack::run_corrimg(){
	//build reg folder
	if(!exists(data_dir+"/reg/"))
		create_directory(data_dir+"/reg/");

	//loop all files
	directory_iterator end_iter;
	std::vector<path> paths;
	for(auto iter = directory_iterator(safe_path(data_dir+"/proj/")); iter!=end_iter; ++iter)
		paths.push_back(iter->path());
	#pragma omp parallel for
	for(auto i=0; i<paths.size(); ++i){
		if(is_regular_file(paths[i])){
			std::string current_file = paths[i].string();
			//find "iii" and "XX" in pattern "../../iii-projXX.nrrd"
		  std::regex pattern("(\\w{3})-proj(\\w{2})");
		  if(std::regex_search(current_file, pattern)){
		    std::smatch results;
		    std::regex_search(current_file, results, pattern);
		    std::string iii = results[1], XX = results[2];

		    //call corrimg
		    corrimgOptions corrimg_opt;
		    corrimg_opt.input_file = current_file;
		    corrimg_opt.output_file =  data_dir + "/reg/" + iii + "-" + XX + ".png";

		    Corrimg(corrimg_opt).main();
  		}
  	}
  }
}


void Pack::run_corrfind(){
  tmax = find_tmax();

  #pragma omp parallel for
  for(auto i=0; i<=tmax; ++i){
  	corrfindOptions corrfind_opt;
  	corrfind_opt.file_dir = data_dir + "/reg/";
  	corrfind_opt.file_number = i;

  	Corrfind(corrfind_opt).main();
  }
}


void Pack::run_corrnhdr(){
	//build nhdr-corr folder
	if(!exists(data_dir+"/nhdr-corr/"))
		create_directory(data_dir+"/nhdr-corr/");

 	corrnhdrOptions corrnhdr_opt;
 	corrnhdr_opt.file_dir = data_dir;
 	corrnhdr_opt.num = find_tmax();

 	Corrnhdr(corrnhdr_opt).main();
}


void Pack::run_anim(std::string nhdr, std::string anim, std::string proj){
	//build anim folder
	if(!exists(data_dir+anim))
		create_directory(data_dir+anim);

	Xml_getter x(safe_path(data_dir+"/xml/000.xml").string());

	animOptions anim_opt;
	anim_opt.tmax = find_tmax();
	anim_opt.nhdr_path = data_dir + nhdr;
	anim_opt.proj_path = data_dir + proj;
	anim_opt.anim_path = data_dir + anim;
	anim_opt.dwn_sample = 2; // TODO: How to decide down_samp??
	anim_opt.scale_x = std::stof(x("ScalingX"))*1e7;
	anim_opt.scale_z = std::stof(x("ScalingZ"))*1e7;

	Anim(anim_opt).main();	//parallelized in function
}


void Pack::run_untext(){
	//build untext folder for projections
	if(!exists(data_dir+"/proj-untext/"))
		create_directory(data_dir+"/proj-untext/");

	tmax = find_tmax();

	#pragma omp parallel for
	for(auto i=0; i<=tmax; ++i)
		for(std::string d: {"XY", "YZ"}){
			untextOptions untext_opt;
			untext_opt.input = data_dir + "/proj/" + zero_pad(i, 3) + "-proj" + d + ".nrrd";
			untext_opt.output = data_dir + "/proj-untext/" + zero_pad(i, 3) + "-proj" + d + ".nrrd";

			Untext(untext_opt).main();
		}
}


void Pack::run_nhdrcheck(){}
void Pack::run_all(){
	//temporary
	run_skim();
	std::cout << "skim done!" << std::endl;
	run_untext();
	std::cout << "untext done!" << std::endl;
	run_corrimg();
	std::cout << "corrimg done!" << std::endl;
	run_corrfind();
	std::cout << "corrfind done!" << std::endl;
	run_corrnhdr();
	std::cout << "corrnhdr done!" << std::endl;
	run_anim("/nhdr-corr/", "/anim-untext/", "/proj-untext/");
	std::cout << "anim done!" << std::endl;
}


void Pack::main(){
	std::string cmd = opt.command;
	if(cmd == "skim"){
		run_skim();
	}
	else if(cmd == "anim")
		run_anim("/nhdr/", "/anim/", "/proj/");
	else if(cmd == "corrimg")
		run_corrimg();
	else if(cmd == "corrfind")
		run_corrfind();
	else if(cmd == "corrnhdr")
		run_corrnhdr();
	else if(cmd == "anim_corr"){
		run_anim("/nhdr-corr/", "/anim-corr/", "/proj/");
	}
	else if(cmd == "untext")
		run_untext();
	else if(cmd == "anim_untext"){
		run_anim("/nhdr-corr/", "/anim-untext/", "/proj-untext/");
	}
	else if(cmd == "all")
		run_all();
	else
		throw LSPException("Unrecognized command.", "pack.cpp", "Pack::main");
}
