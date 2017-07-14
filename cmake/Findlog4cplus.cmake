####################################################################################################
#  This file is a find module script for CMake to locate all required headers and
#  static libraries and to correctly setup Met.3D on every system
#
#  Package: log4cplus
#       - log4cplus_FOUND        - System has found the package
#       - log4cplus_INCLUDE_DIR  - Package include directory
#       - log4cplus_LIBRARIES    - Package static libraries
#
#  Copyright 2016 Marc Rautenhaus
#  Copyright 2016 Michael Kern
#
####################################################################################################

# Include common settings such as common include/library directories
include("cmake/common_settings.cmake")

# Set the name of the package
set (PKG_NAME log4cplus)

# If the system is unix, try to use the pkg_config for a custom library
if (UNIX)
    use_pkg_config(${PKG_NAME})
endif (UNIX)

find_path(${PKG_NAME}_INCLUDE_DIR
        NAMES # Name of all header files
            log4cplus/logger.h
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
            log4cplus LOG4CPLUS
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
            log4cplusd LOG4CPLUSD
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
    set(${PKG_NAME}_LIBRARY_DEBUG "${PKG_NAME}_LIBRARY_RELEASE")
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
# Marks cmake cached variables as advanced
mark_as_advanced(${PKG_NAME}_INCLUDE_DIR ${PKG_NAME}_LIBRARIES)