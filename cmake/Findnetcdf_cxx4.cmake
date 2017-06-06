####################################################################################################
#  This file is a find module script for CMake to locate all required headers and
#  static libraries and to correctly setup Met.3D on every system
#
#  Package: netcdf_cpp4
#       - netcdf_cxx4_FOUND          - System has found the package
#       - netcdf_cxx4_INCLUDE_DIR    - Package include directory
#       - netcdf_cxx4_LIBRARIES      - Package static libraries
#       - netcdf_cxx4_C_INCLUDE_DIR  - Dependant package include directory
#       - netcdf_cxx4_C_LIBRARIES    - Dependant package static libraries
#
#  Copyright 2016 Marc Rautenhaus
#  Copyright 2016 Michael Kern
#
####################################################################################################

# Include common settings such as common include/library directories
include("cmake/common_settings.cmake")

# Set the name of the package
set (PKG_NAME netcdf_cxx4)

# If the system is unix, try to use the pkg_config for a custom library
if (UNIX)
    use_pkg_config(${PKG_NAME})
endif (UNIX)

find_path(${PKG_NAME}_C_INCLUDE_DIR
        NAMES # Name of all header files
            netcdf.h
        HINTS # Hints to the directory where the package is currently installed in
            $ENV{${PKG_NAME}_C_DIR}
            ${PKG_INCLUDE_DIRS}
        PATH_SUFFIXES # Subfolders in the install directory
            include
        PATHS # The directories where cmake should look for the files by default if HINTS does not work
            ${COMMON_INSTALL_DIRS}
        )

find_path(${PKG_NAME}_INCLUDE_DIR
        NAMES # Name of all header files
            netcdf
        HINTS # Hints to the directory where the package is currently installed in
            $ENV{${PKG_NAME}_DIR}
            ${PKG_INCLUDE_DIRS}
        PATH_SUFFIXES # Subfolders in the install directory
            include
        PATHS # The directories where cmake should look for the files by default if HINTS does not work
            ${COMMON_INSTALL_DIRS}
        )

# We also require the c library of this package
find_library(${PKG_NAME}_C_LIBRARY_RELEASE
        NAMES
            netcdf NETCDF
        HINTS
            $ENV{${PKG_NAME}_C_DIR}
            ${PKG_LIBRARY_DIRS}
        PATH_SUFFIXES
            lib64
            lib
        PATHS
            ${COMMON_INSTALL_DIRS}
        )

find_library(${PKG_NAME}_C_LIBRARY_DEBUG
        NAMES
            netcdfd NETCDFd
        HINTS
            $ENV{${PKG_NAME}_C_DIR}
            ${PKG_LIBRARY_DIRS}
        PATH_SUFFIXES
            lib64
            lib
        PATHS
            ${COMMON_INSTALL_DIRS}
        )

find_library(${PKG_NAME}_LIBRARY_RELEASE
        NAMES
            netcdf_c++4 NETCDF_C++4
            netcdf-cxx4
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
            netcdf_c++4d NETCDF_C++4d
            netcdf-cxx4d
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
            netcdf-cxx4.pc
        HINTS
            $ENV{${PKG_NAME}_DIR}
            ${PKG_LIBRARY_DIRS}
        PATH_SUFFIXES
            lib64/pkgconfig
            lib/pkgconfig
        PATHS
            ${COMMON_INSTALL_DIRS}
        )

find_file(${PKG_NAME}_SETTINGS_FILE
        NAMES
            libnetcdf-cxx.settings
        HINTS
            $ENV{${PKG_NAME}_DIR}
            ${PKG_LIBRARY_DIRS}
        PATH_SUFFIXES
            lib64/pkgconfig
            lib/pkgconfig
            lib
        PATHS
            ${COMMON_INSTALL_DIRS}
        )

if (NOT ${PKG_NAME}_C_LIBRARY_DEBUG)
    set(${PKG_NAME}_C_LIBRARY_DEBUG "${PKG_NAME}_C_LIBRARY_RELEASE")
endif()

if (${PKG_NAME}_C_LIBRARY_DEBUG AND ${PKG_NAME}_C_LIBRARY_RELEASE)
    # use different libraries for different configurations
    set (${PKG_NAME}_C_LIBRARIES
            optimized ${${PKG_NAME}_C_LIBRARY_RELEASE}
            debug ${${PKG_NAME}_C_LIBRARY_DEBUG})
endif ()

if (NOT ${PKG_NAME}_LIBRARY_DEBUG)
  set(${PKG_NAME}_LIBRARY_DEBUG "${PKG_NAME}_LIBRARY_RELEASE")
endif()

if (${PKG_NAME}_LIBRARY_DEBUG AND ${PKG_NAME}_LIBRARY_RELEASE)
    # use different libraries for different configurations
    set (${PKG_NAME}_LIBRARIES
            optimized ${${PKG_NAME}_LIBRARY_RELEASE}
            debug ${${PKG_NAME}_LIBRARY_DEBUG})
endif ()

if(WIN32)
  # Parse .settings file
  file(STRINGS ${${PKG_NAME}_SETTINGS_FILE} SETTINGS)
  foreach(SETTING ${SETTINGS})
    string(REPLACE ":" ";" SETTING ${SETTING})
    list(GET SETTING 0 LABEL)
    string(STRIP ${LABEL} LABEL)
    
    if(${LABEL} STREQUAL "NetCDF Version")
      list(GET SETTING 1 ${PKG_NAME}_VERSION)
      string(STRIP ${${PKG_NAME}_VERSION} ${PKG_NAME}_VERSION)      
    endif()
  endforeach()
else()
  # Parse .pc file to get used version of netcdf.
  pkg_check_modules(${PKG_NAME} REQUIRED ${${PKG_NAME}_PC_FILE})
endif()

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set ${PGK_NAME}_FOUND to TRUE if
# all listed variables are TRUE
find_package_handle_standard_args(${PKG_NAME} REQUIRED_VARS ${PKG_NAME}_VERSION ${PKG_NAME}_LIBRARIES ${PKG_NAME}_C_LIBRARIES ${PKG_NAME}_C_INCLUDE_DIR ${PKG_NAME}_INCLUDE_DIR)


# Extract major and minor version from version string.
string(REPLACE "." ";" ${PKG_NAME}_VERSION ${${PKG_NAME}_VERSION})
list(GET ${PKG_NAME}_VERSION 0 ${PKG_NAME}_VERSION_MAJOR)
list(GET ${PKG_NAME}_VERSION 1 ${PKG_NAME}_VERSION_MINOR)
list(GET ${PKG_NAME}_VERSION 2 ${PKG_NAME}_VERSION_PATCH)


# Marks cmake cached variables as advanced
mark_as_advanced(${PKG_NAME}_INCLUDE_DIR ${PKG_NAME}_LIBRARIES ${PKG_NAME}_C_LIBRARIES)
