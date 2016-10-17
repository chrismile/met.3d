
# Locate glfx library
# This module defines
#  GLFX_FOUND, if false, do not try to link to NETCDF_CPP4
#  GLFX_LIBRARIES
#  GLFX_INCLUDE_DIR

find_path(GLFX_INCLUDE_DIR
  NAMES glfx.h
  HINTS
  $ENV{GLFX_DIR}
  PATH_SUFFIXES include/GL
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

find_library(GLFX_LIBRARIES
    NAMES glfx GLFX
    HINTS
    $ENV{GLFX_DIR}
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

INCLUDE(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set GLFX_FOUND to TRUE if 
# all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(glfx DEFAULT_MSG GLFX_LIBRARIES GLFX_INCLUDE_DIR)

MARK_AS_ADVANCED(GLFX_INCLUDE_DIR GLFX_LIBRARIES)

