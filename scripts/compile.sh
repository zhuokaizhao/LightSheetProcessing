# this CMakeLists.txt file is used for server at dali-login.rcc.uchicago.edu

module load boost/1.62.0 gcc/6.1 libpng/1.5 cmake teem libxml2 opencv/3.1 openmpi fftw3

#CXX=$(which g++)
CXX=g++
LSP_path=$(cd $(dirname $0); cd ..; pwd -P)
build_path=$LSP_path/LSP-BUILD
install_path=$LSP_path/LSP-INSTALL

mkdir -p $build_path $install_path

cd $build_path
cmake -DCMAKE_CXX_COMPILER=$CXX \
	-DCMAKE_INSTALL_PREFIX=$install_path\
	$LSP_path

#make install (we can't make this on server)
make install

# write informations into ~/.bash_profile
if [[ ":$PATH:" != *":$install_path/bin:"* ]]; then
	echo "Add lsp information to the end of ~/. bash_profile"
	echo "# Here start writing lsp into \$PATH" >> ~/.bash_profile
	echo "module load boost/1.62.0 gcc/6.1 cmake teem libxml2 opencv/3.1 openmpi fftw3" >> ~/.bash_profile
	echo "PATH=\$PATH:\$HOME/bin:$install_path/bin" >> ~/.bash_profile
	echo "export PATH" >> ~/.bash_profile
	echo "# Complete writeing lsp information" >> ~/.bash_profile

	echo "# Here start writing lsp into \$PATH" >> ~/.profile
	echo "module load boost/1.62.0 gcc/6.1 cmake teem libxml2 opencv/3.1 openmpi fftw3" >> ~/.profile
	echo "PATH=\$PATH:\$HOME/bin:$install_path/bin" >> ~/.profile
	echo "export PATH" >> ~/.profile
	echo "# Complete writeing lsp information" >> ~/.profile
fi
