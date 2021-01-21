####################################################################################################
#  This file is a find module script for CMake to locate all required headers and
#  static libraries and to correctly setup Met.3D on every system
#
#  Package: GLFX
#       - GLFX_FOUND        - System has found the package
#       - GLFX_INCLUDE_DIR  - Package include directory
#       - GLFX_LIBRARIES    - Package static libraries
#
#  Copyright 2016-2017 Marc Rautenhaus
#  Copyright 2016      Michael Kern
#  Copyright 2017      Bianca Tost
#
####################################################################################################

# Include common settings such as common include/library directories
include("cmake/common_settings.cmake")

# Set the name of the package
set (PKG_NAME glfx)

# If the system is unix, try to use the pkg_config for a custom library
if (UNIX)
    use_pkg_config(${PKG_NAME})
endif (UNIX)

find_path(${PKG_NAME}_INCLUDE_DIR
    NAMES # Name of all header files
        GL/glfx.h
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
        glfx GLFX
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
        glfxd GLFXD
    HINTS
        $ENV{${PKG_NAME}_DIR}
        ${PKG_LIBRARY_DIRS}
    PATH_SUFFIXES
        lib64
        lib
    PATHS
        ${COMMON_INSTALL_DIRS}
)

find_file(${PKG_NAME}_PC_FILE
        NAMES
            glfx.pc
        HINTS
            $ENV{${PKG_NAME}_DIR}
            ${PKG_LIBRARY_DIRS}
        PATH_SUFFIXES
            lib64/pkgconfig
            lib/pkgconfig
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
elseif (${PKG_NAME}_LIBRARY_RELEASE)
    # if only release has been found, use that
    set (${PKG_NAME}_LIBRARIES
            ${${PKG_NAME}_LIBRARY_RELEASE})
endif ()

if(WIN32)
    # TODO Is there a way to get the version number of the glfx library under Windows?
    # Mark library version as not found.
    SET(${PKG_NAME}_VERSION "x.x.x")
else()
    # Parse .pc file to get used version of glfx.
    pkg_check_modules(${PKG_NAME}_MOD REQUIRED ${${PKG_NAME}_PC_FILE})
endif()

if(${PKG_NAME}_MOD_VERSION)
  # Extract major, minor and patch version from version string.
  string(REPLACE "." ";" ${PKG_NAME}_TMP_VERSION ${${PKG_NAME}_MOD_VERSION})
  SET(${PKG_NAME}_VERSION ${${PKG_NAME}_MOD_VERSION})
  list(GET ${PKG_NAME}_TMP_VERSION 0 ${PKG_NAME}_VERSION_MAJOR)
  list(GET ${PKG_NAME}_TMP_VERSION 1 ${PKG_NAME}_VERSION_MINOR)
  list(GET ${PKG_NAME}_TMP_VERSION 2 ${PKG_NAME}_VERSION_PATCH)
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set ${PKG_NAME}_FOUND to TRUE if
# all listed variables are TRUE
find_package_handle_standard_args(${PKG_NAME} REQUIRED_VARS ${PKG_NAME}_LIBRARIES ${PKG_NAME}_INCLUDE_DIR)

# Marks cmake cached variables as advanced
mark_as_advanced(${PKG_NAME}_INCLUDE_DIR ${PKG_NAME}_LIBRARIES)