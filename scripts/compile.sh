module load gcc/6.1 cmake teem libxml2 boost

CXX=$(which g++)
outer_path=$(cd $(dirname $0); cd ../..; pwd)
build_path=$outer_path/LSP-BUILD
install_path=$outer_path/LSP-INSTALL

mkdir -p $build_path $install_path

cd $build_path
cmake -DCMAKE_CXX_COMPILER=$CXX \
	-DCMAKE_INSTALL_PREFIX=$install_path\
	$outer_path/LSP

make install
