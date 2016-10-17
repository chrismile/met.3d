
# Locate qcustomplot library
# This module defines
#  QCUSTOMPLOT_FOUND, if false, do not try to link to NETCDF_CPP4
#  QCUSTOMPLOT_LIBRARIES
#  QCUSTOMPLOT_INCLUDE_DIR

find_path(QCUSTOMPLOT_INCLUDE_DIR
  NAMES qcustomplot.h
  HINTS
  $ENV{QCUSTOMPLOT_DIR}
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

find_library(QCUSTOMPLOT_LIBRARY_RELEASE
    NAMES qcustomplot QCUSTOMPLOT
    HINTS
    $ENV{QCUSTOMPLOT_DIR}
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

find_library(QCUSTOMPLOT_LIBRARY_DEBUG
    NAMES qcustomplotd
    HINTS
    $ENV{QCUSTOMPLOT_DIR}
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

IF(QCUSTOMPLOT_LIBRARY_RELEASE AND QCUSTOMPLOT_LIBRARY_DEBUG)
    # use different libraries for different configurations
set (QCUSTOMPLOT_LIBRARY
		optimized 	${QCUSTOMPLOT_LIBRARY_RELEASE}
		debug 		${QCUSTOMPLOT_LIBRARY_DEBUG})
ENDIF()

INCLUDE(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set LOG4CPLUS_FOUND to TRUE if 
# all listed variables are TRUE
FIND_PACKAGE_HANDLE_STANDARD_ARGS(qcustomplot DEFAULT_MSG QCUSTOMPLOT_LIBRARY QCUSTOMPLOT_INCLUDE_DIR)

MARK_AS_ADVANCED(QCUSTOMPLOT_INCLUDE_DIR QCUSTOMPLOT_LIBRARIES)

