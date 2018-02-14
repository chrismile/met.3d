####################################################################################################
#  This file is a find module script for CMake to locate all required headers and
#  static libraries and to correctly setup Met.3D on every system
#
#  Package: GDAL
#       - GDAL_FOUND        - System has found the package
#       - GDAL_INCLUDE_DIR  - Package include directory
#       - GDAL_LIBRARIES    - Package static libraries
#
#  Copyright 2018 Marc Rautenhaus
#  Copyright 2018 Michael Kern
#  Copyright 2018 Bianca Tost
#
####################################################################################################

# Include common settings such as common include/library directories
include("cmake/common_settings.cmake")

# Set the name of the package
set (PKG_NAME GDAL)

find_package(GDAL REQUIRED)
include_directories(${GDAL_INCLUDE_DIR})
# Check for gdal version >= 2.0
file(STRINGS "${GDAL_INCLUDE_DIR}/gdal_version.h" ${PKG_NAME}_VERSION_FILE NEWLINE_CONSUME)
# Extract version from content by matching string: '"major.minor.patch"'.
string(REGEX MATCH "\"[0-9]+([.][0-9]+)?([.][0-9]+)?\"" ${PKG_NAME}_VERSION ${${PKG_NAME}_VERSION_FILE})
string(REPLACE "\"" "" ${PKG_NAME}_VERSION ${${PKG_NAME}_VERSION})
if(${${PKG_NAME}_VERSION} VERSION_LESS "2.0")
    message(FATAL_ERROR "GDAL library version " ${${PKG_NAME}_VERSION} " is not supported by Met.3D. Please update GDAL library to version 2.0 or later.")
endif()