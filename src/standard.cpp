//
//Created by Jiawei Jiang at 06/27/2018
//

#include <boost/filesystem.hpp>
#include <regex>
#include "CLI11.hpp"

#include "skimczi.h"
#include "anim.h"
#include "nhdrCheck.h"
#include "untext.h"
#include "corrimg.h"
#include "corrfind.h"
#include "corrnhdr.h"

#include "standard.h"

using namespace boost::filesystem;

void setup_standard(CLI::App &app){
	auto opt = std::make_shared<standardOptions>();
	auto sub = app.add_subcommand("standard", "Process dataset with standard format");

	sub->add_option("directory", opt->data_dir, "Where the 'czi' folder is")->required();
	sub->add_option("-c, --command", opt->command, "Specify the command you want to run. (Default: all)");

	sub->set_callback([opt](){
		try{
			Standard(*opt).main();
		}
		catch(LSPException &e){
			std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
		}
	});
}


Standard::Standard(standardOptions const &opt): opt(opt){
  //better change path to absolute and resolve links if neccessary.
  data_dir = canonical(opt.data_dir).string();
}


path Standard::safe_path(std::string const &folder){
  //check if path is vaild.
  if(!exists(folder))
    throw LSPException("Error finding path: "+ folder, "standard.cpp", "Standard::safe_path");

  return path(folder);
}


void Standard::main(){
	std::string cmd = opt.command;
	if(cmd == "skim")
		run_skim();
	else if(cmd == "anim")
		run_anim();
	else if(cmd == "nhdrcheck")
		run_nhdrcheck();
	else if(cmd == "untext")
		run_untext();
	else if(cmd == "corrimg")
		run_corrimg();
	else if(cmd == "corrfind")
		run_corrfind();
	else if(cmd == "corrnhdr")
		run_corrnhdr();
	else if(cmd == "all")
		run_all();
	else
		throw LSPException("Unrecognized command.", "standard.cpp", "Standard::main");
}


void Standard::run_skim(){
	//build related folders
	for(std::string str: {"/nhdr/", "/proj/", "/xml/"}){
		if(!exists(data_dir+str))
			create_directory(data_dir+str);
	}

	//loop all files
	directory_iterator end_iter;
	for(directory_iterator iter(safe_path(data_dir+"/czi/")); iter!=end_iter; ++iter){
		if(is_regular_file(iter->path())){
			std::string current_file = iter->path().string();
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
  		skim_opt.po = data_dir + "/proj/" + iii;
		Skim(skim_opt).main();
		}
	}
}


void Standard::run_corrimg(){
	//build reg folder
	if(!exists(data_dir+"/reg/"))
		create_directory(data_dir+"/reg/");

	//loop all files
	directory_iterator end_iter;
	for(directory_iterator iter(safe_path(data_dir+"/proj/")); iter!=end_iter; ++iter){
		if(is_regular_file(iter->path())){
			std::string current_file = iter->path().string();
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


void Standard::run_corrfind(){
	//loop all files, find the max file number
	int max = 0;
	directory_iterator end_iter;
	for(directory_iterator iter(safe_path(data_dir+"/reg/")); iter!=end_iter; ++iter){
		if(is_regular_file(iter->path())){
			std::string current_file = iter->path().string();
			//find "iii" and "XX" in pattern "../../iii-projXX.nrrd"
		  std::regex pattern("(\\w{3})-.*png");
		  if(std::regex_search(current_file, pattern)){
		    std::smatch results;
		    std::regex_search(current_file, results, pattern);
		    int num = std::stoi(results[1]);
		    if(num > max)
		    	max = num;
  		}
  	}
  }
  //call corrfind
  for(auto i=0; i<=max; ++i){
  	corrfindOptions corrfind_opt;
  	corrfind_opt.file_dir = data_dir + "/reg/";
  	corrfind_opt.file_number = i;

  	Corrfind(corrfind_opt).main();
  }
}


void Standard::run_corrnhdr(){
	//loop all files, find the max file number
	int max = 0;
	directory_iterator end_iter;
	for(directory_iterator iter(safe_path(data_dir+"/reg/")); iter!=end_iter; ++iter){
		if(is_regular_file(iter->path())){
			std::string current_file = iter->path().string();
			//find "iii" and "XX" in pattern "../../iii-projXX.nrrd"
		  std::regex pattern("(\\w{3})-.*png");
		  if(std::regex_search(current_file, pattern)){
		    std::smatch results;
		    std::regex_search(current_file, results, pattern);
		    int num = std::stoi(results[1]);
		    if(num > max)
		    	max = num;
  		}
  	}
  }

 	corrnhdrOptions corrnhdr_opt;
 	corrnhdr_opt.file_dir = data_dir;
 	corrnhdr_opt.num = max;

 	Corrnhdr(corrnhdr_opt).main();
}


void Standard::run_anim(){}
void Standard::run_nhdrcheck(){}
void Standard::run_untext(){}
void Standard::run_all(){
	//temporarily
	run_skim();
	run_corrimg();
	run_corrfind();
	run_corrnhdr();
}
