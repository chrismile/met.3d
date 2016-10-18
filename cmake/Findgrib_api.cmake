# Locate grib_api library
# This module defines
#  GRIB_API_FOUND, if false, do not try to link to Log4cplus
#  GRIB_API_LIBRARIES
#  GRIB_API_INCLUDE_DIR

FIND_PATH(GRIB_API_INCLUDE_DIR 
    NAMES grib_api.h
    HINTS
    $ENV{GRIB_API_DIR}
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

FIND_LIBRARY(GRIB_API_LIBRARIES
    NAMES GRIB_API grib_api
    HINTS
    $ENV{GRIB_API_DIR}
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
# handle the QUIETLY and REQUIRED arguments and set GRIB_API_FOUND to TRUE if 
# all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(grib_api DEFAULT_MSG GRIB_API_LIBRARIES GRIB_API_INCLUDE_DIR)

MARK_AS_ADVANCED(GRIB_API_INCLUDE_DIR GRIB_API_LIBRARIES)
