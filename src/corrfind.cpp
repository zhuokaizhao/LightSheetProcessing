//
// Created by Jake Stover on 5/9/18.
// Modified by Zhuokai Zhao
//

#include <boost/filesystem.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <corr.h>

#include "util.h"
#include "skimczi.h"

#include "corrfind.h"

#include <chrono> 

using namespace std;

void setup_corrfind(CLI::App &app) 
{
    auto opt = std::make_shared<corrfindOptions>();

    auto sub = app.add_subcommand("corrfind", "Computes the shift between images with sequence numbers i and i-1. Expects filenames to be similar to 001-{XY,XZ,YX}.png");

    sub->add_option("-i, --image_path", opt->image_path, "Input path that contains images where shifts are going to be found")->required();
    sub->add_option("-o, --align_path", opt->align_path, "Output path that will contain optimal alignment results")->required();
    //sub->add_option("-f, --file_number", opt->file_number, "File number. This command expects files to be of the form reg/%d-{XY,XZ,YX}.png.")->required();
    //sub->add_option("-o, --output", opt->output_name, "Base name to use when saving out the optimal alignemnt of images. (Default: -corr1.txt)");
    
    // optional arguments
    sub->add_option("-k, --kernels", opt->kernel, "Kernels to pass to lsp corr. (Default: c4hexic c4hexicd)")->expected(2);
    sub->add_option("-b, --bound", opt->bound, "Max offset to be passed to lsp corr. (Default: 10)");
    sub->add_option("-e, --epsilon", opt->epsilon, "Epsilon to be passed to lsp corr. (Default: 0.00000000000001)");
    sub->add_option("-v, --verbose", opt->verbose, "Print processing message or not. (Default: 0(close))");

    sub->set_callback([opt]() 
    {
        try 
        {
            // check if input_path is valid, notice that there is no Single file mode for this task, has to be directory
            if (checkIfDirectory(opt->image_path))
            {
                cout << "Input path " << opt->image_path << " is valid, start processing" << endl << endl;

                // map that has key to be type (XY, XZ, YZ), and each key corresponds to a vector of pair of sequence number and string
                // unordered_map< string, vector< pair<int, string> > > inputImages;
                // vector< vector< pair<int, string> > > inputImages;
                // we know that there are two types of images, max and avg
                vector< pair<int, string> > xyImages, xzImages, yzImages;
                
                // number of images of each type
                int numXYImages = 0, numXZImages = 0, numYZImages = 0;

                // get all images from the input anim path
                const vector<string> images = GetDirectoryFiles(opt->image_path);
                for (int i = 0; i < images.size(); i++)
                {
                    // get the current file
                    string curImage = images[i];
                    
                    // check if input file is a .png file
                    int end = curImage.rfind(".png");
                    
                    // if this is indeed an image
                    if ( (end != string::npos) && (end == curImage.length() - 4) )
                    {
                        string curImageName = curImage.substr(0, end);
                        if (opt->verbose)
                            cout << "Current input file " + curImage + " ends with .png, count this image" << endl;
                        
                        // check if the type of current image is max or avg
                        int isXY = curImage.rfind("XY");
                        int isXZ = curImage.rfind("XZ");
                        int isYZ = curImage.rfind("YZ");
                        
                        // if this is a XY image
                        if ( (isXY != string::npos) && (isXZ == string::npos) && (isYZ == string::npos) )
                        {
                            numXYImages++;
                            // now we need to understand the sequence number of this file
                            int start = -1;
                            
                            // current image name without type
                            int sequenceEnd = curImage.rfind("-projXY");
                            
                            // The sequenceNumString will have zero padding, like 001
                            for (int i = 0; i < sequenceEnd; i++)
                            {
                                // we get the first position that zero padding ends
                                if (curImage[i] != '0')
                                {
                                    start = i;
                                    break;
                                }
                            }
                
                            string sequenceNumString;
                            // for the case that it is just 000 which represents the initial time stamp
                            if (start == -1)
                            {
                                sequenceNumString = "0";
                            }
                            else
                            {
                                int sequenceLength = sequenceEnd - start;
                                sequenceNumString = curImage.substr(start, sequenceLength);
                            }

                            if (is_number(sequenceNumString))
                            {
                                xyImages.push_back( make_pair(stoi(sequenceNumString), curImageName) );
                                //cout << sequenceNumString << ", " << curImageName << " has been added" << endl;
                            }
                            else
                            {
                                cout << "WARNING: " << sequenceNumString << " is NOT a number" << endl;
                            }
                        }
                        // if this is a XZ image
                        else if ( (isXY == string::npos) && (isXZ != string::npos) && (isYZ == string::npos) )
                        {
                            numXZImages++;
                            // now we need to understand the sequence number of this file
                            int start = -1;
                            
                            // current image name without type
                            int sequenceEnd = curImage.rfind("-projXZ");

                            // The sequenceNumString will have zero padding, like 001
                            for (int i = 0; i < sequenceEnd; i++)
                            {
                                // we get the first position that zero padding ends
                                if (curImage[i] != '0')
                                {
                                    start = i;
                                    break;
                                }
                            }
                
                            string sequenceNumString;
                            // for the case that it is just 000 which represents the initial time stamp
                            if (start == -1)
                            {
                                sequenceNumString = "0";
                            }
                            else
                            {
                                int sequenceLength = sequenceEnd - start;
                                sequenceNumString = curImage.substr(start, sequenceLength);
                            }

                            if (is_number(sequenceNumString))
                            {
                                xzImages.push_back( make_pair(stoi(sequenceNumString), curImageName) );
                                //cout << sequenceNumString << ", " << curImageName << " has been added" << endl;
                            }
                            else
                            {
                                cout << "WARNING: " << sequenceNumString << " is NOT a number" << endl;
                            }
                        }
                        // if this is a YZ image
                        else if ( (isXY == string::npos) && (isXZ == string::npos) && (isYZ != string::npos) )
                        {
                            numYZImages++;
                            // now we need to understand the sequence number of this file
                            int start = -1;
                            
                            // current image name without type
                            int sequenceEnd = curImage.rfind("-projYZ");

                            // The sequenceNumString will have zero padding, like 001
                            for (int i = 0; i < sequenceEnd; i++)
                            {
                                // we get the first position that zero padding ends
                                if (curImage[i] != '0')
                                {
                                    start = i;
                                    break;
                                }
                            }
                
                            string sequenceNumString;
                            // for the case that it is just 000 which represents the initial time stamp
                            if (start == -1)
                            {
                                sequenceNumString = "0";
                            }
                            else
                            {
                                int sequenceLength = sequenceEnd - start;
                                sequenceNumString = curImage.substr(start, sequenceLength);
                            }

                            if (is_number(sequenceNumString))
                            {
                                yzImages.push_back( make_pair(stoi(sequenceNumString), curImageName) );
                                //cout << sequenceNumString << ", " << curImageName << " has been added" << endl;
                            }
                            else
                            {
                                cout << "WARNING: " << sequenceNumString << " is NOT a number" << endl;
                            }
                        }
                        // something wrong with the input images' namings
                        else
                        {
                            cout << "Input images are NOT in the correct namings, program stops" << endl;
                            return;
                        }
                    }
                    // if this file is not an image, do nothing
                    else 
                    {
                        
                    }
                }

                // sanity check - numXYImages should equal to the size of xyImages
                if (numXYImages != xyImages.size())
                {
                    cout << "ERROR when loading xxx-projXY images due to unexpected naming, program stops" << endl;
                    return;
                }
                // sanity check - numXZImages should equal to the size of xzImages
                if (numXZImages != xzImages.size())
                {
                    cout << "ERROR when loading xxx-projXZ images due to unexpected naming, program stops" << endl;
                    return;
                }
                // sanity check - numYZImages should equal to the size of yzImages
                if (numYZImages != yzImages.size())
                {
                    cout << "ERROR when loading xxx-projYZ images due to unexpected naming, program stops" << endl;
                    return;
                }
                // sanity check - we need to have the same number of XY, XZ and YZ images
                if ( (numXYImages != numXZImages) || (numXYImages != numYZImages) || (numXZImages != numYZImages) )
                {
                    cout << "ERROR -projXY, -projXZ, and -projYZ should have the same number of images, program stops" << endl;
                    cout << "numXYImages = " << numXYImages << endl;
                    cout << "numXZImages = " << numXZImages << endl;
                    cout << "numYZImages = " << numYZImages << endl;
                    return;
                }

                // sort xyImages, xzImages and yzImages in ascending order based on their sequence numbers
                sort(xyImages.begin(), xyImages.end());
                sort(xzImages.begin(), xzImages.end());
                sort(yzImages.begin(), yzImages.end());

                cout << endl << numXYImages << " -projXY images found in input path " << opt->image_path << endl;
                cout << numXZImages << " -projXZ images found in input path " << opt->image_path << endl;
                cout << numYZImages << " -projYZ images found in input path " << opt->image_path << endl;

                opt->inputImages.push_back(xyImages);
                opt->inputImages.push_back(xzImages);
                opt->inputImages.push_back(yzImages);

                for (int i = 0; i < opt->inputImages[0].size(); i++)
                {
                    for (int j = 0; j < opt->inputImages.size(); j++)
                    {
                        cout << opt->inputImages[j][i].second << endl;
                    }
                }

                // run the Corrfind
                Corrfind(*opt).main();
            }
            else
            {
                cout << "Input path is invalid, program exits" << endl;
            }
            
        } 
        catch(LSPException &e) 
        {
            std::cerr << "Exception thrown by " << e.get_func() << "() in " << e.get_file() << ": " << e.what() << std::endl;
        }
    });
}


Corrfind::Corrfind(corrfindOptions const &opt): opt(opt), mop(airMopNew()) {}


Corrfind::~Corrfind()
{
    airMopOkay(mop);
}


void Corrfind::main() 
{
    // create output directory if not exist
    if (!checkIfDirectory(opt.align_path))
    {
        boost::filesystem::create_directory(opt.align_path);
        cout << "Output path " << opt.align_path << " does not exits, but has been created" << endl;
    }

    // Process the images by pair can call corrfind
    for (int i = 0; i < opt.inputImages[0].size(); i++)
    {
        // generate opt for corr
        corrOptions opt_corr;
        opt_corr.output_file = opt.align_path + GenerateOutName(i, 3, ".txt");
        opt_corr.verbose = opt.verbose;
        opt_corr.kernel = opt.kernel;
        opt_corr.max_offset = opt.bound;
        opt_corr.epsilon = opt.epsilon;

        // each time stamp only has one output txt file
        ofstream outfile(opt_corr.output_file);
        // when i == 0, there is no i-1 for correlation
        if (i == 0)
        {
            outfile << std::vector<double>{0, 0, 0, 0} << std::endl;
        }
        else
        {
            // all the correlation results of current time stamp
            vector< vector<double> > allShifts;

            // for each TYPE of images, we want to find correlation between i-1 and i, so i starts with 1
            for (int j = 0; j < opt.inputImages.size(); j++)
            {
                opt_corr.input_images.push_back(opt.image_path + opt.inputImages[j][i].second + ".png");
                opt_corr.input_images.push_back(opt.image_path + opt.inputImages[j][i-1].second + ".png");

                // then we can run corr_main
                cout << endl << "Currently processing between " << opt_corr.input_images[0] << " and " << opt_corr.input_images[1] << endl;
                auto start = chrono::high_resolution_clock::now();

                std::vector<double> curShift = corr_main(opt_corr);
                allShifts.push_back(curShift);

                auto stop = chrono::high_resolution_clock::now(); 
                auto duration = chrono::duration_cast<chrono::seconds>(stop - start); 
                cout << "Processing took " << duration.count() << " seconds" << endl; 
                
            }

            // we take the average of the top 2 xx/yy/zz as the final result
            double xx = (allShifts[0][0] + allShifts[0][2])/2.0;
            double yy = (allShifts[1][1] + allShifts[1][4])/2.0;
            double zz = (allShifts[2][3] + allShifts[2][5])/2.0;

            outfile << std::vector<double>{xx, yy, zz, AIR_CAST(double, i)} << std::endl;
        }

        // close the output file of current time stamp
        outfile.close();


    }
}