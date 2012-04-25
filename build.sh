#!/bin/sh
# Build script for NANDFlashSim

# Builder comment ID
NFS_BUILDER_INFO_COMMENT="[NFS_BUILDER][INFO] "
NFS_BUILDER_ERROR_COMMENT="[NFS_BUILDER][ERROR] "

# Builder options
NFS_BUILDER_USER_CFLAGS="-g"
NFS_BUILDER_OPT_DEFINE="-DNO_STORAGE -DNO_ENFORCING_ORDER -DENFORCING_NOP -WITHOUT_PLANE_STATS"
NFS_BUILDER_CUR_PATH="${PWD}"
NFS_BUILDER_NFS_PATH="${NFS_BUILDER_CUR_PATH}/nfs"
NFS_BUILDER_BOOST_PATH="/home/mj/Dev/boost_1_48_0"
NFS_BUILDER_BOOST_BOOTSTRAP="bootstrap.sh"
NFS_BUILDER_BOOST_BJAM="bjam"
NFS_BUILDER_BOOST_STAGE="stage"
NFS_BUILDER_BOOST_STAGE_PATH="${NFS_BUILDER_BOOST_PATH}/${NFS_BUILDER_BOOST_STAGE}"
NFS_BUILDER_BOOST_LIB_PATH="${NFS_BUILDER_BOOST_STAGE_PATH}/lib"
NFS_BUILDER_TARGET_FILE_NAME="NANDFlashSim"
NFS_BUILDER_STATIC_LIB_FILE_NAME="libnfs.a"
NFS_BUILDER_SHARED_LIB_SO_NAME="libnfs.so.1"
NFS_BUILDER_SHARED_LIB_FILE_NAME="${NFS_BUILDER_SHARED_LIB_SO_NAME}.0.1"

echo "=============== NANDFlashSim Builder ==============="
echo "| This builder needs bellow items                  |"
echo "| OS                                               |"
echo "| - Ubuntu 11.4   (tested)                         |"
echo "| External Libraries                               |"
echo "| - zlib1g-dev    (for Boost.IOStreams)            |"
echo "| - libbz2-dev    (for Boost.IOStreams)            |"
echo "| Utils                                            |"
echo "| - gccmakedep    (for source dependency)          |"
echo "| Free Space                                       |"
echo "| - least 600MB                                    |"
echo "===================================================="
echo ""

# If you have bellow package. You can comment bellow apt-get lines
# Install essential package
echo ${NFS_BUILDER_INFO_COMMENT}"Install essential packages."
# gccmakedep
sudo apt-get install xutils-dev
# zlib
sudo apt-get install zlib1g-dev
# libbz2
sudo apt-get install libbz2-dev
echo ""

# Build boost library
echo ${NFS_BUILDER_INFO_COMMENT}"Check the boost library."
echo ${NFS_BUILDER_INFO_COMMENT}"Target boost : "${NFS_BUILDER_BOOST_PATH}
cd ${NFS_BUILDER_BOOST_PATH}
if [ -e ${NFS_BUILDER_BOOST_BJAM} ]
then
	echo ${NFS_BUILDER_INFO_COMMENT}"The bjam util is checked."
else
	echo ${NFS_BUILDER_INFO_COMMENT}"It's first time. Build the boost util."
	./${NFS_BUILDER_BOOST_BOOTSTRAP} --prefix=${NFS_BUILDER_BOOST_PATH}
fi
if [ -d ${NFS_BUILDER_BOOST_STAGE} ]
then
	echo ${NFS_BUILDER_INFO_COMMENT}"The boost library is checked."
else
	echo ${NFS_BUILDER_INFO_COMMENT}"Build the boost static library."
	./${NFS_BUILDER_BOOST_BJAM} link=static ${NFS_BUILDER_BOOST_STAGE}
fi
echo ""

# Build NANDFlashSim
echo ${NFS_BUILDER_INFO_COMMENT}"Build the NANDFlashSim project."
cd ${NFS_BUILDER_NFS_PATH}
if [ -z ${1} ]
then
	# There is no option parameter
	# Make executable file of NANDFlashSim
	echo ${NFS_BUILDER_INFO_COMMENT}"Make executable file of NANDFlashSim."
	make execnfs TARGET_FILE_NAME="${NFS_BUILDER_TARGET_FILE_NAME}" INCLUDE_PATH="${NFS_BUILDER_BOOST_PATH}" LIB_PATH="${NFS_BUILDER_BOOST_LIB_PATH}" TARGET_CFLAGS="${NFS_BUILDER_USER_CFLAGS}" TARGET_DEFINE="${NFS_BUILDER_OPT_DEFINE}"
	if [ -e ${NFS_BUILDER_TARGET_FILE_NAME} ]
	then
		cp -f ${NFS_BUILDER_TARGET_FILE_NAME} ${NFS_BUILDER_CUR_PATH}
	else
		echo ""
		echo ${NFS_BUILDER_ERROR_COMMENT}"Occur build error!!!"
		echo ""
		exit
	fi
	# Clean-up
	echo ${NFS_BUILDER_INFO_COMMENT}"Clean-up the NANDFlashSim project."
	make clean TARGET_FILE_NAME="${NFS_BUILDER_TARGET_FILE_NAME}"
	echo ""
elif [ ${1} = "--static-lib" ]
then
	# There is --static-lib option parameter
	# Make static library file of NANDFlashSim
	echo ${NFS_BUILDER_INFO_COMMENT}"Make static library file of NANDFlashSim."
	make slib TARGET_FILE_NAME="${NFS_BUILDER_STATIC_LIB_FILE_NAME}" INCLUDE_PATH="${NFS_BUILDER_BOOST_PATH}" LIB_PATH="${NFS_BUILDER_BOOST_LIB_PATH}" TARGET_CFLAGS="${NFS_BUILDER_USER_CFLAGS}" TARGET_DEFINE="${NFS_BUILDER_OPT_DEFINE}"
	if [ -e ${NFS_BUILDER_STATIC_LIB_FILE_NAME} ]
	then
		cp -f ${NFS_BUILDER_STATIC_LIB_FILE_NAME} ${NFS_BUILDER_CUR_PATH}
	else
		echo ""
		echo ${NFS_BUILDER_ERROR_COMMENT}"Occur build error!!!"
		echo ""
		exit
	fi
	# Clean-up
	echo ${NFS_BUILDER_INFO_COMMENT}"Clean-up the NANDFlashSim project."
	make clean TARGET_FILE_NAME="${NFS_BUILDER_STATIC_LIB_FILE_NAME}"
	echo ""
elif [ ${1} = "--shared-lib" ]
then
	# Three is --shared-lib option parameter
	# Make shared library file of NANDFlashSim
	echo ${NFS_BUILDER_INFO_COMMNET}"Make shared library file of NANDFlashSim"
	make rlib TARGET_FILE_NAME="${NFS_BUILDER_SHARED_LIB_FILE_NAME}" INCLUDE_PATH="${NFS_BUILDER_BOOST_PATH}" LIB_PATH="${NFS_BUILDER_BOOST_LIB_PATH}" TARGET_CFLAGS="${NFS_BUILDER_USER_CFLAGS} -fPIC" RLIB_FLAGS="-shared -W1,-soname,${NFS_BUILDER_SHARED_LIB_SO_NAME}" TARGET_DEFINE="${NFS_BUILDER_OPT_DEFINE}"
	if [ -e ${NFS_BUILDER_SHARED_LIB_FILE_NAME} ]
	then
		cp -f ${NFS_BUILDER_SHARED_LIB_FILE_NAME} ${NFS_BUILDER_CUR_PATH}
	else
		echo ""
		echo ${NFS_BUILDER_ERROR_COMMENT}"Occur build error!!!"
		echo ""
		exit
	fi
	# Clean-up
	echo ${NFS_BUILDER_INFO_COMMENT}"Clean-up the NANDFlashSim project."
	make clean TARGET_FILE_NAME="${NFS_BUILDER_SHARED_LIB_FILE_NAME}"
	echo ""
else
	echo ""
	echo ${NFS_BUILDER_ERROR_COMMENT}"Invalid option parameter."
	echo ""
fi
echo ""

# Exit builder
cd ${NFS_BUILDER_NFS_PATH}
echo ""
