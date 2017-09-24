
# Locate netcdf_c++4 library
# This module defines
#  NETCDF_CPP4_FOUND, if false, do not try to link to NETCDF_CPP4
#  NETCDF_CPP4_LIBRARIES
#  NETCDF_CPP4_INCLUDE_DIR

find_path(NETCDF_CPP4_INCLUDE_DIR
  NAMES netcdf
  HINTS
  $ENV{NETCDF_CPP4_DIR}
  PATH_SUFFIXES include
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local
  /usr
  /sw # Fink
  /opt/local # DarwinPorts
  /opt/csw # Blastwave
  /opt
)

# we always require the c-library of netcdf
find_library(NETCDF_C_LIBRARY
    NAMES netcdf
    HINTS
    $ENV{NETCDF_DIR}
    PATH_SUFFIXES lib64 lib
    PATHS
    ~/Library/Frameworks
    /Library/Frameworks
    /usr/local
    /usr
    /sw
    /opt/local
    /opt/csw
    /opt
)

find_library(NETCDF_CPP4_LIBRARY
  NAMES netcdf_c++4 NETCDF_C++4
  HINTS
  $ENV{NETCDF_CPP4_DIR}
  PATH_SUFFIXES lib64 lib
  PATHS
  ~/Library/Frameworks
  /Library/Frameworks
  /usr/local
  /usr
  /sw
  /opt/local
  /opt/csw
  /opt
)

IF(NETCDF_C_LIBRARY AND NETCDF_CPP4_LIBRARY)
    set(NETCDF_CPP4_LIBRARIES ${NETCDF_C_LIBRARY} ${NETCDF_CPP4_LIBRARY})
ENDIF()

INCLUDE(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LOG4CPLUS_FOUND to TRUE if 
# all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(netcdf_cpp4 DEFAULT_MSG NETCDF_CPP4_LIBRARIES NETCDF_CPP4_INCLUDE_DIR)

MARK_AS_ADVANCED(NETCDF_CPP4_INCLUDE_DIR NETCDF_CPP4_LIBRARIES)
