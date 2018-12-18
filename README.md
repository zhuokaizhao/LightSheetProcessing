# LightSheetProcessing
LightSheetProcessing (LSP) is a software that processes image data captured by a lightsheet microscope. LSP reads a number of or one specific microscope image data file created in the Carl Zeiss CZI format, which saves image stacks, time lapse series and tile images captured from a Carl Zeiss microscope. LSP first organized them into detached-header [NRRD File format](http://teem.sourceforge.net/nrrd/format.html) and later generates PNG images as well as corresponding AVI videos based on the Maximum Intensity Projection (MIP) and Average Intensity Projection (AIP) of the input data.

## Dependencies
1. Compiler:
<br />C++ 11
2. Standard libraries (Specifically required libraries showed in parentheses):
<br />Libxml2 [link](http://xmlsoft.org/)
<br />Boost (boost_system, boost_filesystem) [link](https://www.boost.org/)
<br />OpenCV3 (opencv_core, opencv_videoio, opencv_imgcodecs, opencv_imgproc) [link] (https://opencv.org/opencv-3-4-1.html)
<br />FFTW3 [link](http://www.fftw.org/) 
<br />libpng [link](http://www.libpng.org/pub/png/libpng.html) 
<br />zlib [link](https://zlib.net/)
3. Customized libraries:
<br />Teem [link](http://teem.sourceforge.net/)
    
## Compilation
Under the main directory `LightSheetProcessing/`, run "compile.sh" under `scripts/`
`sh scripts/compile.sh`		

Note: The script is written to be run on Linux system, modification required if running on other platforms. By default it will add the install path `~/LightSheetProcessing/LSP-INSTALL/` to your  `~/.bash_profile` and `~/.profile`

## Standard input data format
1. All files should be in the Carl Zeiss CZI format (.czi files).
2. If you want LSP to process a number of consecutive files, please put all of them under one path (for example, `~/czi/*.czi`). And name them to have the following format, note that the first (0 timestamp) should have no parentheses.
`data_name.czi, data_name(1).czi, data_name(2).czi, ......`

## Running
1. Run `lsp -h` for help.    
2. LSP currently includes four subcommands: `lsp skim`, `lsp proj`, `lsp anim` and `lsp start`. Same to the general command `lsp`, each subcommand could be run to show help instructions when added `-h` flag as well.     

### lsp skim
LSP Skim provides utilities for getting information out of CZI files and organizes them into detached-header NRRD file format. More specifically, it generates .nhdr NRRD header files to permit extracting the image and essential meta data from CZI file.

### lsp proj
### lsp anim
### lsp start
 