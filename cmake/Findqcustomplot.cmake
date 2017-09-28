####################################################################################################
#  This file is a find module script for CMake to locate all required headers and
#  static libraries and to correctly setup Met.3D on every system
#
#  Package: qcustomplot
#       - qcustomplot_FOUND        - System has found the package
#       - qcustomplot_INCLUDE_DIR  - Package include directory
#       - qcustomplot_LIBRARIES    - Package static libraries
#
#  Copyright 2016 Marc Rautenhaus
#  Copyright 2016 Michael Kern
#
####################################################################################################

# Include common settings such as common include/library directories
include("cmake/common_settings.cmake")

# Set the name of the package
set (PKG_NAME qcustomplot)

# If the system is unix, try to use the pkg_config for a custom library
if (UNIX)
    use_pkg_config(${PKG_NAME})
endif (UNIX)

find_path(${PKG_NAME}_INCLUDE_DIR
        NAMES # Name of all header files
            qcustomplot.h
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
            qcustomplot QCUSTOMPLOT
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
            qcustomplotd QCUSTOMPLOTD
        HINTS
            $ENV{${PKG_NAME}_DIR}
            ${PKG_LIBRARY_DIRS}
        PATH_SUFFIXES
            lib64
            lib
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
# handle the QUIETLY and REQUIRED arguments and set ${PGK_NAME}_FOUND to TRUE if
# all listed variables are TRUE
find_package_handle_standard_args(${PKG_NAME} REQUIRED_VARS ${PKG_NAME}_LIBRARIES ${PKG_NAME}_INCLUDE_DIR)

# Extract version number of QCustomPlot library from header-file:
# Get content of header-file.
file(STRINGS "${qcustomplot_INCLUDE_DIR}/qcustomplot.h" ${PKG_NAME}_VERSION
     NEWLINE_CONSUME)
# Extract version from content by matching string: "Version: major.minor.patch".
string(REGEX MATCH "Version: [0-9][.][0-9][.][0-9]"
       ${PKG_NAME}_VERSION ${${PKG_NAME}_VERSION})
# Extract version from matched string by using the white space to create list and
# choosing the secong argument of the list.
string(REPLACE " " ";" ${PKG_NAME}_VERSION ${${PKG_NAME}_VERSION})
list(GET ${PKG_NAME}_VERSION 1 ${PKG_NAME}_VERSION)
# Split version into major, minor and patch part.
string(REPLACE "." ";" ${PKG_NAME}_VERSION ${${PKG_NAME}_VERSION})
# Set major version.
list(GET ${PKG_NAME}_VERSION 0 ${PKG_NAME}_MAJOR_VERSION)
# Set minor version.
list(GET ${PKG_NAME}_VERSION 1 ${PKG_NAME}_MINOR_VERSION)
# Set patch version.
list(GET ${PKG_NAME}_VERSION 2 ${PKG_NAME}_PATCH_VERSION)

# Marks cmake cached variables as advanced
mark_as_advanced(${PKG_NAME}_INCLUDE_DIR ${PKG_NAME}_LIBRARIES)