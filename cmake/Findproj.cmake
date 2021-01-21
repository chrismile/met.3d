####################################################################################################
#  This file is a find module script for CMake to locate all required headers and
#  static libraries and to correctly setup Met.3D on every system
#
#  Package: proj
#       - proj_FOUND        - System has found the package
#       - proj_INCLUDE_DIR  - Package include directory
#       - proj_LIBRARIES    - Package static libraries
#
#  Copyright 2020 Kameswarro Modali
#
####################################################################################################

# Include common settings such as common include/library directories
include("cmake/common_settings.cmake")

# Set the name of the package
set (PKG_NAME proj)

# If the system is unix, try to use the pkg_config for a custom library
if (UNIX)
    use_pkg_config(${PKG_NAME})
endif (UNIX)

find_path(${PKG_NAME}_INCLUDE_DIR
        NAMES # Name of all header files
            proj_api.h
	    projects.h
        HINTS # Hints to the directory where the package is currently installed in
            $ENV{${PKG_NAME}_DIR}
            ${PKG_INCLUDE_DIRS}
        PATH_SUFFIXES # Subfolders in the install directory
            include
        PATHS # The directories where cmake should look for the files by default if HINTS does not work
            ${COMMON_INSTALL_DIRS}
        )

find_library(${PKG_NAME}_LIBRARY_RELEASE
        NAMES
	proj PROJ
        HINTS
            $ENV{${PKG_NAME}_DIR}
            ${PKG_LIBRARY_DIRS}
        PATH_SUFFIXES
            lib64
            lib
        PATHS
            ${COMMON_INSTALL_DIRS}
        )

find_library(${PKG_NAME}_LIBRARY_DEBUG
        NAMES
	proj PROJD
        HINTS
            $ENV{${PKG_NAME}_DIR}
            ${PKG_LIBRARY_DIRS}
        PATH_SUFFIXES
	    usr/lib/debug/usr/lib64
	    #lib64
	    #lib
        PATHS
            ${COMMON_INSTALL_DIRS}
        )

if (NOT ${PKG_NAME}_LIBRARY_DEBUG)
    message("Debug library for ${PKG_NAME} was not found")
    set(${PKG_NAME}_LIBRARY_DEBUG "${${PKG_NAME}_LIBRARY_RELEASE}")
endif()

if (NOT ${PKG_NAME}_LIBRARY_RELEASE)
    message("Release library for ${PKG_NAME} was not found")
    set(${PKG_NAME}_LIBRARY_RELEASE "${${PKG_NAME}_LIBRARY_DEBUG}")
endif()

if (${PKG_NAME}_LIBRARY_DEBUG AND ${PKG_NAME}_LIBRARY_RELEASE)
    # use different libraries for different configurations
    set (${PKG_NAME}_LIBRARIES
            optimized ${${PKG_NAME}_LIBRARY_RELEASE}
            debug ${${PKG_NAME}_LIBRARY_DEBUG})
endif ()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set ${PKG_NAME}_FOUND to TRUE if
# all listed variables are TRUE
find_package_handle_standard_args(${PKG_NAME} REQUIRED_VARS ${PKG_NAME}_LIBRARIES ${PKG_NAME}_INCLUDE_DIR)

# Extract version number of Proj library from header-file:
# Get content of header-file.
#file(STRINGS "${${PKG_NAME}_INCLUDE_DIR}/proj_api.h" ${PKG_NAME}_VERSION NEWLINE_CONSUME)
file(STRINGS "${${PKG_NAME}_INCLUDE_DIR}/proj_api.h" ${PKG_NAME}_VERSION_STRING NEWLINE_CONSUME)
# Extract version from content by matching string: "Version: major.minor.patch".
#string(REGEX MATCH "Version [0-9]+([.][0-9]+)?([.][0-9]+)?"
#       ${PKG_NAME}_VERSION ${${PKG_NAME}_VERSION})
string(REGEX MATCH "#define PJ_VERSION ([0-9])*"
       ${PKG_NAME}_VERSION_STRING ${${PKG_NAME}_VERSION_STRING})
# Extract version from matched string by using the white space to create list and
# choosing the second argument of the list.
#string(REPLACE " " ";" ${PKG_NAME}_VERSION ${${PKG_NAME}_VERSION})
#list(GET ${PKG_NAME}_VERSION 1 ${PKG_NAME}_VERSION)
string(REGEX REPLACE "#define PJ_VERSION ([0-9])([0-9])([0-9])"
        "\\1;\\2;\\3" ${PKG_NAME}_VERSION_LIST ${${PKG_NAME}_VERSION_STRING})
list(LENGTH ${PKG_NAME}_VERSION_LIST n)
set(${PKG_NAME}_VERSION, "")
math(EXPR n "${n} - 1")
foreach(i RANGE ${n})
    list(GET ${PKG_NAME}_VERSION_LIST ${i} item)
    if(${i} EQUAL 0)
        string(CONCAT ${PKG_NAME}_VERSION ${${PKG_NAME}_VERSION} ${item})
    else()
        string(CONCAT ${PKG_NAME}_VERSION ${${PKG_NAME}_VERSION} "." ${item})
    endif()
endforeach()

if(${PKG_NAME}_VERSION VERSION_LESS "4.0")
    message(FATAL_ERROR "Proj library version " ${${PKG_NAME}_VERSION} " is not supported by Met.3D. Please update Proj library to version 4.0 or later.")
endif()

# Split version into major, minor and patch part.
#string(REPLACE "." ";" ${PKG_NAME}_VERSION ${${PKG_NAME}_VERSION})
# Set major version.
list(GET ${PKG_NAME}_VERSION_LIST 0 ${PKG_NAME}_MAJOR_VERSION)
# Set minor version.
list(GET ${PKG_NAME}_VERSION_LIST 1 ${PKG_NAME}_MINOR_VERSION)
# Set patch version.
list(GET ${PKG_NAME}_VERSION_LIST 2 ${PKG_NAME}_PATCH_VERSION)

# Marks cmake cached variables as advanced
mark_as_advanced(${PKG_NAME}_INCLUDE_DIR ${PKG_NAME}_LIBRARIES)
