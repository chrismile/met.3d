# *****************************************************************************
#   CMAKE project file for Met.3D
#
#   This file is part of Met.3D -- a research environment for the
#   three-dimensional visual exploration of numerical ensemble weather
#   prediction data.
#
#   Copyright (c) 2016 Marc Rautenhaus
#   Copyright (c) 2016 Michael Kern <kernm@in.tum.de>
#
#   Computer Graphics and Visualization Group
#   Technische Universitaet Muenchen, Garching, Germany
#
#   Met.3D is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   Met.3D is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with Met.3D.  If not, see <http://www.gnu.org/licenses/>.
#
# *****************************************************************************

# Set cmake requirement
cmake_minimum_required(VERSION 3.5.1)

# Tell cmake where to look for our scripts
set(CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)

##############################################################################
#                         Project Description                                #
##############################################################################

# Set project name
project(Met.3D)
# Set version number 
set(Met3D_VERSION_MAJOR 1)
set(Met3D_VERSION_MINOR 1)

# Add global defines
add_definitions(-DENABLE_MET3D_STOPWATCH
                -DENABLE_HYBRID_PRESSURETEXCOORDTABLE
                -DENABLE_RAYCASTER_ACCELERATION
                -DDIRECT_SYNCHRONIZATION
                -DQCUSTOMPLOT_USE_LIBRARY)

# Add global options
option(USE_CLANG "Build application with clang" OFF)

if (WIN32)
    # TODO: support for windows
    #set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build/win32 CACHE PATH "Build directory" FORCE)
endif (WIN32)

if (UNIX)
    #set(EXECUTABLE_OUTPUT_PATH ${CMAKE_CURRENT_SOURCE_DIR}/build/unix CACHE PATH "Build directory" FORCE)
    #set(RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/build/unix)
    #set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build/unix")
endif(UNIX)


# Set flags if we use GNUCC
if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
# Or CLANG
# TODO: clang command arguments slightly differ from GNUCC
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang" OR USE_CLANG)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")
endif ()


##############################################################################
#                      Create Configure File                                 #
##############################################################################

# configure a header file to pass some CMake settings to the source code
#configure_file("${PROJECT_SOURCE_DIR}/CMakeConfig.h.in"
#	       "${PROJECT_BINARY_DIR}/CMakeConfig.h"
#		)

# add the binary tree to the include directory paths
#include_directories("${PROJECT_BINARY_DIR}")


##############################################################################
#                      Required Qt4 Libraries                                #
##############################################################################

# TODO! does not work with cmake command in terminal
# Automocs are generated and stored in the directory from where the user
# has called the make command by default

#set_property(GLOBAL PROPERTY USE_FOLDERS ON)
#set_property(GLOBAL PROPERTY AUTOGEN_TARGETS_FOLDER ${CMAKE_CURRENT_SOURCE_DIR}/automoc)

# Automatically build Qt moc files, wrap UIs and add RCC resource files
set(CMAKE_AUTOMOC ON) # instead of using the macro QT4_WRAP_CPP
#set(CMAKE_AUTOUIC ON) # instead of using the macro QT4_WRAP_UI, still buggy
#set(CMAKE_AUTORCC ON) # instead of using the macro QT4_ADD_RESOURCES
set(CMAKE_INCLUDE_CURRENT_DIR ON)
cmake_policy(SET CMP0020 NEW)

find_package(Qt4 4.8 REQUIRED QTCORE QTGUI QTOPENGL QTXML QTNETWORK)
# Future: Change to Qt5.
#find_package(Qt5Core REQUIRED)
#find_package(Qt5Xml REQUIRED)
#find_package(Qt5XmlPatterns REQUIRED)
#find_package(Qt5Widgets REQUIRED)

# Import Qt headers.
INCLUDE(${QT_USE_FILE})

# Qt4 forms / designer forms
file (GLOB_RECURSE  MET3D_UIS 	src/*.ui)
# Qt4 resources
file (GLOB_RECURSE  MET3D_RCS   ../*.qrc)

# Build cpp and header from UI forms
qt4_wrap_ui(MET3D_UIS_H ${MET3D_UIS})
# Add QT resources
qt4_add_resources(MET3D_RC_SRCS ${MET3D_RCS})
# Future: Change to Qt5.
#qt5_use_modules(OpenGL Xml XmlPatterns)


##############################################################################
#                  Required Third-Party Libraries                            #
##############################################################################

# For all libraries that do not have "Find..."-scripts provided in 
# /usr/share/cmake/Modules, corresponding scripts need to be provided in
# cmake/...

# Freetype
find_package(Freetype REQUIRED)
include_directories(${FREETYPE_INCLUDE_DIRS})

# OpenGL packages
find_package(OpenGL REQUIRED)
find_package(GLU REQUIRED)
find_package(GLEW REQUIRED)
# TODO: glew is not supported with OpenGL widget (which is deprecated anyway)
include_directories(${GLEW_INCLUDE_DIRS})

# Log4cplus
find_package(log4cplus REQUIRED)
include_directories(${log4cplus_INCLUDE_DIR})

# GDAL
find_package(GDAL REQUIRED)
include_directories(${GDAL_INCLUDE_DIR})

# GSL
find_package(GSL REQUIRED)
include_directories(${GSL_INCLUDE_DIRS})

# Grib-API
find_package(grib_api REQUIRED)
include_directories(${grib_api_INCLUDE_DIR})

# Netcdf
find_package(netcdf_cxx4 REQUIRED)
include_directories(${netcdf_cxx4_INCLUDE_DIR})

# Add global defines for netcdf cxx4 library version checking.
add_definitions(-DNetCDFCXX4_VERSION_MAJOR=${netcdf_cxx4_VERSION_MAJOR}
                -DNetCDFCXX4_VERSION_MINOR=${netcdf_cxx4_VERSION_MINOR})

# QCustomPlot
find_package(qcustomplot REQUIRED)
include_directories(${qcustomplot_INCLUDE_DIR})
# Add major version as preprocessor definition.
add_definitions(-DQCPLOT_MAJOR_VERSION=${qcustomplot_MAJOR_VERSION})

# GLFX
find_package(glfx REQUIRED)
include_directories(${glfx_INCLUDE_DIR})

# OpenMP
find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

# Eigen3
#find_package(Eigen3 REQUIRED)
#include_directories(${EIGEN3_INCLUDE_DIR})


##############################################################################
#                      Configure Other Projects                              #
##############################################################################

# add QtProperty Browser
# add_subdirectory()
# include_directories()

#add_subdirectory(external/qtpropertybrowser)
#include_directories(external/qtpropertybrowser/src)


##############################################################################
#                     Configure Met.3D Project                               #
##############################################################################

# Source files.
file (GLOB_RECURSE  Met3D_Header    src/*.h)
file (GLOB_RECURSE  Met3D_Source    src/*.cpp)
file (GLOB_RECURSE  Met3D_Source_C  src/*.c)
file (GLOB          Met3D_Shaders   src/glsl/*)
file (GLOB          Met3D_Configs   config/*)

#source_group(automocs FILES ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_automoc.cpp)

# NOTE: we could also use file (GLOB_RECURSE Met3D_Source src/*) but we still
# want to keep control over what has to be included in the project

# Third-party souce files.
file (GLOB QT_PROPERTYBROWSER_Source ../third-party/qt-solutions/qtpropertybrowser/src/*)

# Set filters.
if (WIN32)
    source_group(Met3D FILES ${Met3D_Source_Files} ${})
    source_group(Met3D\\Actors FILES ${Met3D_Actors_Source_Files})
endif (WIN32)

# Set include directories.
include_directories(${CMAKE_CURRENT_BINARY_DIR} # since QT files will be located here
                    ${PROJECT_SOURCE_DIR}/src
                    ${PROJECT_SOURCE_DIR}/../third-party/qt-solutions/qtpropertybrowser/src
                    )

# set custom targets to display files not included in the executable build process
add_custom_target(shaders SOURCES ${Met3D_Shaders})
add_custom_target(configs SOURCES ${Met3D_Configs})
add_custom_target(uis SOURCES     ${Met3D_UIS})

# Tell cmake to create Met.3D executable.
add_executable(Met3D # executable name
                ${Met3D_Source} # cpp source files
                ${Met3D_Header} # h header files
                ${Met3D_Source_C} # c source files
               
                ${QT_PROPERTYBROWSER_Source}
                ${MET3D_UIS_H} # add qt uis to the executable
                ${MET3D_RC_SRCS}
  )

# Link Qt4 and other libraries to executable.
target_link_libraries(Met3D
                        Qt4::QtCore
                        Qt4::QtGui
                        Qt4::QtOpenGL
                        Qt4::QtXml
                        Qt4::QtNetwork
                        ${FREETYPE_LIBRARIES}
                        ${OPENGL_gl_LIBRARY}
                        ${OPENGL_glu_LIBRARY}
                        ${GLEW_LIBRARIES}
                        ${GDAL_LIBRARY}
                        ${GSL_LIBRARIES}
                        ${log4cplus_LIBRARIES}
                        ${grib_api_LIBRARIES}
                        ${netcdf_cxx4_LIBRARY}
                        ${netcdf_cxx4_C_LIBRARY}
                        ${qcustomplot_LIBRARIES}
                        ${glfx_LIBRARIES}
                        #${EIGEN3_LIBRARIES}
  )

# Set command line arguments TODO does not work!
#set_target_properties(Met3D PROPERTIES COMMAND_LINE_ARGUMENTS "--pipeline=config/pipeline.cfg --frontend=config/frontend.cfg")

# Set environment paths
set(ENV{MET3D_HOME} "${PROJECT_SOURCE_DIR}")
set(ENV{MET3D_BASE} "${PROJECT_SOURCE_DIR}/../")


##############################################################################
#                           Custom Commands
##############################################################################

# ...
