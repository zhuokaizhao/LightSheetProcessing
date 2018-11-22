//
// Created by Jake Stover on 5/2/18.
//

#include <CLI11.hpp>
#include <util.h>
#include <regex>
#include <nhdrCheck.h>
#include <boost/filesystem.hpp>
#include <boost/range/iterator_range.hpp>

using namespace boost::filesystem;

void setup_nhdr_check(CLI::App &app) 
{
    auto opt = std::make_shared<nhdrCheckOptions>();
    auto sub = app.add_subcommand("nhdrcheck", "Creates line graph summary of nhdr files.");

    sub->add_option("-p, --path", opt->path, "Path to directory containing *-line.nrrd files.");

    sub->set_callback([opt]() 
    {
        try 
        {
            nhdr_check_main(*opt);
        } 
        catch(LSPException &e) 
        {
            std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
        }
    });
}

void nhdr_check_main(nhdrCheckOptions const &opt) 
{
    airArray *mop = airMopNew();

    path proj_dir;
    if (opt.path.empty()) 
    {
        proj_dir = current_path().string() + "/nhdr";
    } 
    else 
    {
        proj_dir = opt.path;
    }

    std::vector<path> line_files;

    try {
        if (is_directory(proj_dir)) 
        {
            std::cout << "Searching for line nrrd files in: " << proj_dir.string() << std::endl;
            
            for (auto& x : boost::make_iterator_range(directory_iterator(proj_dir))) 
            {
                path pstring = x.path();
                if (pstring.string().find("line")) 
                {
                    line_files.push_back(pstring);
                }
            }

    //      std::sort(line_files.begin(), line_files.end(), [] (const path& lhs, const path& rhs) {
    //          int v_lhs = stoi(std::regex_replace(lhs.filename().string(), std::regex(R"([\D])"), ""));
    //          int v_rhs = stoi(std::regex_replace(rhs.filename().string(), std::regex(R"([\D])"), ""));
    //          return v_lhs < v_rhs;
    //      });
        } 
        else 
        {
            throw LSPException(proj_dir.string() + " does not exist or is not a directory.", "nhdrCheck.cpp", "nhdr_check_main");
        }
    } catch (const filesystem_error &err) {
        std::cout << err.what() << std::endl;
    }

    std::cout << "Line files found: " << line_files.size() << std::endl;
}

