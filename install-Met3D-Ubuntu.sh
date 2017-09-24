#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

DONE=${GREEN}[done]${NC}

printf "Installing Met.3D\n"
printf "=================\n\n"

#use bash instead of dash
sudo rm /bin/sh
sudo ln -s /bin/bash /bin/sh

#echo -e "${GREEN}Successful ${RED}Failed ${NC} done\n"

installPackage(){
	# first argument stores package name
	package="$1"
	#printf $package

	printf "\t -> Install ${package} ...\r"
	sudo apt-get install ${package} -y > /dev/null
	printf "\t -> Install ${package} ${DONE}\n"

	return 0
}

installPackages(){
	packs=("${!1}")
	#printf ${packs}

	numPacks=${#packs[@]}
	numPacks=$(( ${numPacks} - 1 ))

	for i in `seq 0 ${numPacks}`
	do 
		#printf "${packs[i]}\n"
		installPackage "${packs[i]}"
	done

	return 0
}

installCustomLib(){
	folder=$1
	urlAddr=$2
	targetDir=$3
	option=$4

	filename=$(basename $urlAddr)
	ext="${filename##*.}"

	if ! test -f "tmp/${filename}"; then 
		#printf "File does not exists"
		printf "\t -> downloading ${filename} ...\r"
		wget -q ${urlAddr} -P ./tmp 
		printf "\t -> downloading ${filename} ${DONE}\n"
	fi;

	#printf ${urlAddr}	
	cd "./tmp"

	printf "\t -> extracting ${filename} ...\r"
	if [ "$ext" == "gz" ]; then
		tar -xf ${filename}
	fi;

	if [ "$ext" == "zip" ]; then
		unzip -q ${filename}
	fi;
	printf "\t -> extracting ${filename} ${DONE}\n"

	cd ${folder}

	printf "\t -> installing ...\r"cdt 
	
	if [ "$4" ] && [ "$option" == "cmake" ]; then
		cmake -DCMAKE_INSTALL_PREFIX:PATH=${targetDir} CMakeLists.txt > /dev/null
	else
		./configure --prefix=${targetDir} > /dev/null	
	fi;
	
	make -j 8 > /dev/null
	sudo make install > /dev/null
	printf "\t -> installing ${DONE}\n"

	cd ..
	
	# remove old files
	sudo rm -r "${folder}"
	sudo rm -r "${filename}"
	# go back to the previous folder
	cd ..

	return 0
}

installQCustomPlot(){
	targetDir=$1

	qplotUrl="http://www.qcustomplot.com/release/1.3.2/QCustomPlot.tar.gz"
	qplotUrlLib="http://www.qcustomplot.com/release/1.3.2/QCustomPlot-sharedlib.tar.gz"

	wget -q ${qplotUrl} -P ./tmp
	wget -q ${qplotUrlLib} -P ./tmp

	cd tmp

	tar -xf QCustomPlot.tar.gz
	tar -xf QCustomPlot-sharedlib.tar.gz -C ./qcustomplot

	cd qcustomplot/qcustomplot-sharedlib/sharedlib-compilation

	qmake
	make -j 8
	sudo make install

	sudo mv *.so* ${targetDir}/lib
	cd ../..
	sudo cp qcustomplot.h ${targetDir}/include

	cd ..

	sudo rm -r qcustomplot
	sudo rm QCustomPlot-sharedlib.tar.gz
	sudo rm QCustomPlot.tar.gz

	cd ..

	return 0
}

# Check if OpenGL version is supported (> 4.3.0)

# Install pre-required / useful packages
reqs=(cmake git unzip ssh openssh-server)
installPackages reqs[@]

# Install Qt4, liblog4cplus, netcdf, glew, freetype, grib-api, gsl
packsQt=("qt4-default qtcreator qt4-qmlviewer" "liblog4cplus-1.1-9 liblog4cplus-dev" \
	"libnetcdf-dev" "glew-utils libglew-dev" "libfreetype6-dev" "libgrib-api-dev libgrib-api-tools" \
	"libglu1-mesa libglu1-mesa-dev" 
	"libgsl-dev" "libgeos-dev" "libeigen3-dev")
installPackages packsQt[@]

# Install custom 
mkdir tmp

# glfx
installCustomLib glfx "https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/glfx/glfx-0.75-src.zip" /usr/local "cmake"

# netcdf-cxx4
installCustomLib netcdf-cxx4-4.2.1 "https://github.com/Unidata/netcdf-cxx4/archive/v4.2.1.tar.gz" /usr/local

# gdal-2.2.1
installCustomLib gdal-2.1.1 "http://download.osgeo.org/gdal/2.1.1/gdal-2.1.1.tar.gz" /usr/local

# hdf5-1.8.17
installCustomLib hdf5-1.8.17 "http://www.hdfgroup.org/ftp/HDF5/current/src/hdf5-1.8.17.tar.gz" /usr/local

# QCustomPlot
installQCustomPlot /usr/local

met3dDir="/home/kerninator/Uni"

cd "${met3dDir}"
mkdir Met.3D-base

cd "Met.3D-base"
mkdir "third-party"

cd "third-party"
# clone QtPropertyBrowser
git clone https://github.com/qtproject/qt-solutions.git

# get TrueType Fonts
wget http://ftp.gnu.org/gnu/freefont/freefont-ttf-20120503.zip
unzip freefont-ttf-20120503.zip
rm freefont-ttf-20120503.zip

# get vector and raster map / coastline and country borderline data
mkdir naturalearth
cd naturalearth

wget http://www.naturalearthdata.com/http//www.naturalearthdata.com/download/50m/physical/ne_50m_coastline.zip
unzip ne_50m_coastline.zip
rm ne_50m_coastline.zip

wget http://www.naturalearthdata.com/http//www.naturalearthdata.com/download/50m/cultural/ne_50m_admin_0_boundary_lines_land.zip
unzip ne_50m_admin_0_boundary_lines_land.zip
rm ne_50m_admin_0_boundary_lines_land.zip

wget http://www.naturalearthdata.com/http//www.naturalearthdata.com/download/50m/raster/HYP_50M_SR_W.zip
unzip HYP_50M_SR_W.zip
rm HYP_50M_SR_W.zip

# checkout Met.3D base directory
username="mchke89"

cd "${met3dDir}/Met.3D-base"
git clone "https://$username@bitbucket.org/wxmetvis/met.3d.devel.git"

cd met.3d.devel
cp met3D_inclib.pri.template met3D_inclib.pri.user
cp config/default_frontend.cfg.template config/frontend.cfg
cp config/default_pipeline.cfg.template config/pipeline.cfg

# set environment variables
Met3DHome="${met3dDir}/Met.3D-base/met.3d.devel"
Met3DBase="${met3dDir}/Met.3D-base"

echo "export MET3D_HOME=$Met3DHome" >> ~/.profile
echo "export MET3D_BASE=$Met3DBase" >> ~/.profile


# Switch back to dash
sudo rm /bin/sh
sudo ln -s /bin/dash /bin/sh


