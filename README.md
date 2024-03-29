# LightSheetProcessing
LightSheetProcessing (LSP) is a software that processes image data captured by a light sheet fluorescence microscopy. LSP reads a number of or one specific microscopy image data file created in the Carl Zeiss CZI format, which saves image stacks, time lapse series and tile images captured from a Carl Zeiss microscope. LSP first organized them into [detached-header NRRD File format](http://teem.sourceforge.net/nrrd/format.html#general.2) and later generates PNG images as well as corresponding AVI videos based on the Maximum Intensity Projection (MIP) and Average Intensity Projection (AIP) of the input data.

## Dependencies
1. **Language**:
- C++ 11
2. **Standard libraries** (Specifically required libraries showed in parentheses):
- [Libxml2](http://xmlsoft.org/)
- [Boost](https://www.boost.org/) (boost_system, boost_filesystem)
- [OpenCV3](https://opencv.org/opencv-3-4-1.html) (opencv_core, opencv_videoio, opencv_imgcodecs, opencv_imgproc)
- [FFTW3](http://www.fftw.org/) 
- [libpng](http://www.libpng.org/pub/png/libpng.html) 
- [zlib](https://zlib.net/)
3. **Customized libraries**:
- [Teem](http://teem.sourceforge.net/)
    
## Compilation
After getting all the dependencies ready, under the main directory `LightSheetProcessing/`, run
```
sh scripts/compile.sh
```		

Note: The script is written to be run on Linux system, modifications are required if running on other platforms. By default it will add the install path `/LightSheetProcessing/LSP-INSTALL/` to your `~/.bash_profile` and `~/.profile`.

## Standard input data format
1. All files should be in the Carl Zeiss CZI format (.czi files).
2. If you want LSP to process a number of consecutive files, please put all of them under one path (for example, `~/czi/*.czi`). And name them to have the following format, note that the first (0 timestamp) should have no parentheses.
```
data_name.czi (0 timestamp data)
data_name(1).czi
data_name(2).czi
...
```

## Running
1. Run `lsp -h` to show the most general information about the program.
2. LSP includes two general processing pipelines - `lsp start` and `lsp start_with_corr`, where "corr" stands for correlation. 
- `lsp start` 
<br /> `lsp start` is the processing pipeline that **should be used when there is NO obvious specimen drift** during the experiment collecting the microscope data. Because it does not include any drift correction algorithm. More specifically, it only combines `lsp skim`, `lsp proj` and `lsp anim`, which are responsible for reading raw CZI data, generating MIP and MIA projection files, and producing final image sequences respectively. Details of these separate programs will be showed in more detail in the next section.
  - Required arguments:
    - `-c, czi_path`, path which contains all the input CZI files
    - `-n, nhdr_path`, path which will contain all the NHDR headers and XML data files generated by `lsp skim`
    - `-p, proj_path`, path which will contain all the NRRD projection files generated by `lsp proj`
    - `-a, anim_path`, path which will contain all the PNG images and AVI videos generated by `lsp anim`
  - Optional arguments:
    - `-f, fps`, frame per second (fps) of the generated AVI video, default is 10
    - `-v, verbose`, 0 for essential progress outputs only, 1 for all the printouts
  - Output formats:
    - NHDR headers and XML data files:
      <br /> All NHDR headers and XML data files will have three-digit names saved into `nhdr_path`, which correspond to their time stamps
      ```
      000.nhdr, 000.xml;
      001.nhdr, 001.xml;
      002.nhdr, 002.xml;
      ...
      ```
    - NRRD projection files:
      <br /> NRRD projection files in all three planes will be saved into `proj_path`, and will have the following format:
      ```
      000-projXY.nrrd, 000-projXZ.nrrd, 000-projYZ.nrrd;
      001-projXY.nrrd, 001-projXZ.nrrd, 001-projYZ.nrrd;
      002-projXY.nrrd, 002-projXZ.nrrd, 002-projYZ.nrrd;
      ...
      ```
    - PNG images and AVI videos:
      - All images will be saved into `anim_path`, and will have the following format, for both `average` and `max` channel:
        ```
        000-avg.png, 000-max.png;
        001-avg.png, 001-max.png;
        002-avg.png, 002-max.png;
        ...
        ```
      - All videos will be saved into `anim_path`, and will have the following format, for both `average` and `max` channel:
        ```
        avg.avi, max.avi
        ```
        If a specific `max_file_number` was entered as argument, the output videos will have names indicating the number:
        ```
        avg_max_file_number.avi, max_max_file_number.avi
        ```
      - There will also be some files ending with `.ppm` and `.nrrd` generated, but are simply the outputs generated in the middle of processing

- `lsp start_with_corr`
<br /> `lsp start_with_corr` is the processing pipeline that **should be used when there IS obvious specimen drift** during the experiment collecting the microscope data. Compared to the standard `lsp start`, it includes drift correction algorithm. More specifically, it not only combines `lsp skim`, `lsp proj` and `lsp anim`, but also includes three subprograms which are responsible for drift correction; `lsp corrimg`, `lsp corrfind` and `lsp corrnhdr`. They are used to generate images for every NRRD projection files in all three directions, calculate the best correlation results between images, and produce final new NHDR headers respectively. These new NHDR headers will then be used to generate new projection files by `lsp proj` and later images and videos by `lsp anim`. With that being said, `lsp start_with_corr` will take a lot more time than `lsp start` because it not only runs additional processings, but also runs `lsp proj` twice.
  - Required arguments:
    - `-c, czi_path`, path which contains all the input CZI files
    - `-n, nhdr_path`, path which will contain all the NHDR headers and XML data files generated by `lsp skim`
    - `-p, proj_path`, path which will contain all the NRRD projection files generated by `lsp proj`
    - `-m, image_path`, path which will contain all the images generated by `lsp corrimg`, which correspond to all projection files in `proj_path`
    - `-r, align_path`, path which will contain all the TXT correlation results generated by `lsp corrfind`
    - `-h, new_nhdr_path`, path which will contain all the new NHDR headers generated by `lsp corrnhdr`
    - `-j, new_proj_path`, path which will contain all the new NRRD projection files generated by `lsp proj` with new NHDR headers in `new_nhdr_path`
    - `-a, anim_path`, path which will contain all the PNG images and AVI videos generated by `lsp anim`
  - Optional arguments:
    - `-f, fps`, frame per second (fps) of the generated AVI video, default is 10
    - `-v, verbose`, 0 for essential progress outputs only, 1 for all the printouts
  - Output formats:
    - NHDR headers and XML data files:
      <br /> Both NHDR headers and XML data files will have three-digit names saved into `nhdr_path`, which correspond to their time stamps
      ```
      000.nhdr, 000.xml;
      001.nhdr, 001.xml;
      002.nhdr, 002.xml;
      ...
      ```
    - NRRD projection files:
      <br /> NRRD projection files in all three planes will be saved into `proj_path`, and will have the following format:
      ```
      000-projXY.nrrd, 000-projXZ.nrrd, 000-projYZ.nrrd;
      001-projXY.nrrd, 001-projXZ.nrrd, 001-projYZ.nrrd;
      002-projXY.nrrd, 002-projXZ.nrrd, 002-projYZ.nrrd;
      ...
      ```
    - PNG images used for computing correlations:
      <br /> All images will be saved into `image_path`, and will have the following format:
      ```
      000-projXY.png, 000-projXZ.png, 000-projYZ.png;
      001-projXY.png, 001-projXZ.png, 001-projYZ.png;
      002-projXY.png, 002-projXZ.png, 002-projYZ.png;
      ...
      ```
    - Correlation results:
      <br /> TXT correlation results will be saved into `align_path`, and will have the following format:
      ```
      000.txt;
      001.txt;
      002.txt;
      ...
      ```
    - New NHDR headers after drift correction:
      <br /> Compared to the old NHDR headers, the new NHDR headers will have modified space origins as the results of drift correction. Same as the old NHDR headers' naming format, they will have three-digit names saved into `new_nhdr_path`, which correspond to their time stamps
      ```
      000.nhdr;
      001.nhdr;
      002.nhdr;
      ...
      ```
    - New NRRD projection files after drift correction:
      <br /> Same as the old NRRD projection files' naming format, all three planes will be saved into `new_proj_path`, and will have the following format:
      ```
      000-projXY.nrrd, 000-projXZ.nrrd, 000-projYZ.nrrd;
      001-projXY.nrrd, 001-projXZ.nrrd, 001-projYZ.nrrd;
      002-projXY.nrrd, 002-projXZ.nrrd, 002-projYZ.nrrd;
      ...
      ```
    - PNG images and AVI videos:
      - All images will be saved into `anim_path`, and will have the following format, for both `average` and `max` channel:
        ```
        000-avg.png, 000-max.png;
        001-avg.png, 001-max.png;
        002-avg.png, 002-max.png;
        ...
        ```
      - All videos will be saved into `anim_path`, and will have the following format, for both `average` and `max` channel:
        ```
        avg.avi, max.avi
        ```
        If a specific `max_file_number` was entered as argument, the output videos will have names indicating the number:
        ```
        avg_max_file_number.avi, max_max_file_number.avi
        ```
      - There will also be some files ending with `.ppm` and `.nrrd` generated, but are simply the outputs generated in the middle of processing

3. Besides the above pipelines, LSP also includes six subcommands: `lsp skim`, `lsp proj`, `lsp anim`, `lsp corrimg`, `lsp corrfind`, and `lsp corrnhdr`. Same to the general command `lsp`, each subcommand could be run to show help instructions when added `-h` flag as well.
- `lsp skim`
<br /> `lsp skim` provides utilities for getting information out of CZI files and organizes them into detached-header NRRD file format. More specifically, it generates NHDR header files to permit extracting the image and essential XML meta data from CZI files.
  - Required arguments:
    - `-i, czi_path`, input path which contains all the input CZI files
    - `-o, nhdr_path`, output path for the generated NHDR headers and .xml data files
    - `-v, verbose`, 0 for essential progress outputs only, 1 for all the printouts
  - Output formats:
    - All NHDR headers and XML data files will have three-digit names saved into `nhdr_path`, which correspond to their time stamps
    ```
    000.nhdr, 000.xml;
    001.nhdr, 001.xml;
    002.nhdr, 002.xml;
    ...
    ```

- `lsp proj`
<br /> `lsp proj` creates NRRD projection files in X-Y, X-Z and Y-Z planes based on NHDR headers and XML data files that were generated by `lsp skim`. 
  - Required arguments:
    - `-i, nhdr_path`, input path which contains all the NHDR headers and XML data files generated by `lsp skim`
    - `-o, proj_path`, output path for the generated NRRD projection files
  - Optional arguments:
    - `-v, verbose`, 0 for essential progress outputs only, 1 for all the printouts
  - Output formats:
    - NRRD projection files in all three planes will have the following format:
    ```
    000-projXY.nrrd, 000-projXZ.nrrd, 000-projYZ.nrrd;
    001-projXY.nrrd, 001-projXZ.nrrd, 001-projYZ.nrrd;
    002-projXY.nrrd, 002-projXZ.nrrd, 002-projYZ.nrrd;
    ...
    ```
- `lsp anim`
<br /> `lsp anim` creates PNG images as well as corresponding AVI videos from NRRD projection files generated by `skim proj`
  - Required arguments:
    - `-i, nhdr_path`, input path which contains all the NHDR headers and XML data files generated by `lsp skim`
    - `-p, proj_path`, input path which contains all the NRRD projection files generated by `lsp proj`
    - `-o, anim_path`, output path for the generated PNG images and AVI videos
  - Optional arguments:
    - `-n, max_file_number`, max number of files that we want to process
    - `-f, fps`, frame per second (fps) of the generated AVI video, default is 10
    - `-d, dsample`, amount by which to down-sample the input data, default is 1.0
    - `-x, scalex`, scaling on the x axis, default: 1.0
    - `-z, scalez`, scaling on the z axis, default: 1.0
    - `-v, verbose`, 0 for essential progress outputs only, 1 for all the printouts
  - Output formats:
    - PNG images will have the following format, for both `average` and `max` channel:
    ```
    000-avg.png, 000-max.png;
    001-avg.png, 001-max.png;
    002-avg.png, 002-max.png;
    ...
    ```
    - AVI video will have the following format, for both `average` and `max` channel:
    ```
    avg.avi, max.avi
    ```
    If a specific `max_file_number` was entered as argument, the output videos will have names indicating the number:
    ```
    avg_max_file_number.avi, max_max_file_number.avi
    ```
    - There will also be some files ending with `.ppm` and `.nrrd` generated, but are simply the outputs generated in the middle of processing

- `lsp corrimg`
<br /> `lsp corrimg` creates images for each NRRD projection file generated by `lsp proj`
  - Required arguments:
    - `-i, proj_path`, input path which contains all the NRRD projection files generated by `lsp proj`
    - `-o, image_path`, output path for the generated corresponding images
  - Optional arguments:
    - `-v, verbose`, 0 for essential progress outputs only, 1 for all the printouts
  - Output formats:
    - All images will be saved into `image_path`, and will have the following format:
      ```
      000-projXY.png, 000-projXZ.png, 000-projYZ.png;
      001-projXY.png, 001-projXZ.png, 001-projYZ.png;
      002-projXY.png, 002-projXZ.png, 002-projYZ.png;
      ...
      ```

- `lsp corrfind`
<br /> `lsp corrfind` computes the shift between images with sequence numbers i and i-1
  - Required arguments:
    - `-i, image_path`, input path for the all the images generated by `lsp corrimg`
    - `-o, align_path`, output path for the generated correlation results
  - Optional arguments:
    - `-k, kernels`, kernels for resampling. Default is (c4hexic c4hexicd)
    - `-b, bound`, max offset for correlation results. Default is 10
    - `-e, epsilon`, convergence for sub-resolution optimization. Default is 0.00000000000001
    - `-v, verbose`, 0 for essential progress outputs only, 1 for all the printouts
  - Output formats:
    - All TXT correlation results will be saved into `align_path`, and will have the following format:
      ```
      000.txt;
      001.txt;
      002.txt;
      ...
      ```

- `lsp corrnhdr`
<br /> `lsp corrnhdr` uses the corrections calculated by `lsp corrfind` to generate new NHDR headers from old NHDR headers
  - Required arguments:
    - `-n, nhdr_path`, input path which contains all the NHDR headers and XML data files generated by `lsp skim`
    - `-c, corr_path`, input path which contains all the correlation results generated by `lsp corrfind`
    - `-o, new_nhdr_path`, output path which will contain the new NHDR headers
  - Optional arguments:
    - `-v, verbose`, 0 for essential progress outputs only, 1 for all the printouts
  - Output formats:
    - Compared to the old NHDR headers, the new NHDR headers will have modified space origins as the results of drift correction. Same as the old NHDR headers' naming format, they will have three-digit names saved into `new_nhdr_path`, which correspond to their time stamps
      ```
      000.nhdr;
      001.nhdr;
      002.nhdr;
      ...
      ```


 