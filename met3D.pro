# *****************************************************************************
#   QMAKE project file for Met.3D
#
#   This file is part of Met.3D -- a research environment for the
#   three-dimensional visual exploration of numerical ensemble weather
#   prediction data.
#
#   Copyright 2015-2018 Marc Rautenhaus
#   Copyright 2016-2018 Bianca Tost
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

TEMPLATE = app

# Include the sources for the QtPropertyBrowser.
include(../third-party/qt-solutions/qtpropertybrowser/src/qtpropertybrowser.pri)

CONFIG += console

QT += opengl core gui network xml
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets printsupport concurrent

HEADERS += \
    src/util/mstopwatch.h \
    src/mainwindow.h \
    src/gxfw/msceneviewglwidget.h \
    src/gxfw/mactor.h \
    src/gxfw/mglresourcesmanager.h \
    src/util/mutil.h \
    src/gxfw/mscenecontrol.h \
    src/actors/graticuleactor.h \
    src/data/nccfvar.h \
    src/util/mexception.h \
    src/gxfw/mtypes.h \
    src/actors/transferfunction1d.h \
    src/gxfw/gl/texture.h \
    src/gxfw/msystemcontrol.h \
    src/data/waypoints/waypointstableview.h \
    src/data/waypoints/waypointstablemodel.h \
    src/data/trajectoryreader.h \
    src/data/trajectories.h \
    src/data/abstractdatareader.h \
    src/data/abstractmemorymanager.h \
    src/data/abstractdatasource.h \
    src/data/trajectorydatasource.h \
    src/gxfw/gl/vertexbuffer.h \
    src/data/structuredgrid.h \
    src/data/abstractdataitem.h \
    src/data/weatherpredictiondatasource.h \
    src/data/memorymanageddatasource.h \
    src/data/lrumemorymanager.h \
    src/data/datarequest.h \
    src/data/floatpertrajectorysource.h \
    src/data/scheduleddatasource.h \
    src/data/scheduler.h \
    src/data/task.h \
    src/data/trajectoryfilter.h \
    src/data/trajectorynormalssource.h \
    src/data/pressuretimetrajectoryfilter.h \
    src/data/deltapressurepertrajectory.h \
    src/data/singletimetrajectoryfilter.h \
    src/data/trajectoryselectionsource.h \
    src/gxfw/gl/abstractgpudataitem.h \
    src/data/probdftrajectoriessource.h \
    src/data/climateforecastreader.h \
    src/data/weatherpredictionreader.h \
    src/gxfw/nwpmultivaractor.h \
    src/gxfw/nwpactorvariable.h \
    src/gxfw/selectdatasourcedialog.h \
    src/data/structuredgridensemblefilter.h \
    src/data/verticalregridder.h \
    src/data/thinouttrajectoryfilter.h \
    src/data/probabilityregiondetector.h \
    src/gxfw/nwpactorvariableproperties.h \
    src/data/valueextractionanalysis.h \
    src/data/abstractanalysis.h \
    src/data/regioncontributionanalysis.h \
    src/util/metroutines.h \
    src/gxfw/gl/shadereffect.h \
    src/data/gribreader.h \
    src/system/applicationconfiguration.h \
    src/gxfw/actorcreationdialog.h \
    src/gxfw/scenemanagementdialog.h \
    src/gxfw/colourmap.h \
    src/data/naturalearthdataloader.h \
    src/gxfw/transferfunction.h \
    src/gxfw/camera.h \
    src/gxfw/textmanager.h \
    src/gxfw/synccontrol.h \
    src/gxfw/gl/typedvertexbuffer.h \
    src/actors/basemapactor.h \
    src/actors/volumebboxactor.h \
    src/actors/trajectoryactor.h \
    src/actors/nwphorizontalsectionactor.h \
    src/actors/nwpverticalsectionactor.h \
    src/actors/nwpsurfacetopographyactor.h \
    src/actors/nwpvolumeraycasteractor.h \
    src/system/qtproperties.h \
    src/actors/movablepoleactor.h \
    src/qt_extensions/qtpropertymanager_extensions.h \
    src/system/frontendconfiguration.h \
    src/system/pipelineconfiguration.h \
    src/data/probabltrajectoriessource.h \
    src/gxfw/gl/shaderstoragebufferobject.h \
    src/gxfw/memberselectiondialog.h \
    src/gxfw/adddatasetdialog.h \
    src/data/derivedmetvarsdatasource.h \
    src/data/bboxtrajectoryfilter.h \
    src/gxfw/mresizewindowdialog.h \
    src/actors/spatial1dtransferfunction.h \
    src/actors/transferfunctioneditor/transferfunctioneditor.h \
    src/actors/transferfunctioneditor/colour.h \
    src/actors/transferfunctioneditor/colourpicker.h \
    src/actors/transferfunctioneditor/editortransferfunction.h \
    src/gxfw/rotatedgridsupportingactor.h \
    src/gxfw/boundingbox/bboxdockwidget.h \
    src/gxfw/boundingbox/boundingbox.h \
    src/gxfw/sessionmanagerdialog.h \
    src/data/singlevariableanalysis.h \
    src/data/structuredgridstatisticsanalysis.h \
    src/qt_extensions/scientificdoublespinbox.h

SOURCES += \
    src/main.cpp \
    src/mainwindow.cpp \
    src/gxfw/msceneviewglwidget.cpp \
    src/gxfw/mactor.cpp \
    src/gxfw/mglresourcesmanager.cpp \
    src/gxfw/mscenecontrol.cpp \
    src/actors/graticuleactor.cpp \
    src/data/nccfvar.cpp \
    src/util/mexception.cpp \
    src/util/mutil.cpp \
    src/actors/transferfunction1d.cpp \
    src/gxfw/gl/texture.cpp \
    src/gxfw/msystemcontrol.cpp \
    src/data/waypoints/waypointstableview.cpp \
    src/data/waypoints/waypointstablemodel.cpp \
    src/data/trajectoryreader.cpp \
    src/data/trajectories.cpp \
    src/data/abstractdatareader.cpp \
    src/data/abstractmemorymanager.cpp \
    src/data/abstractdatasource.cpp \
    src/data/trajectorydatasource.cpp \
    src/gxfw/gl/vertexbuffer.cpp \
    src/data/structuredgrid.cpp \
    src/data/abstractdataitem.cpp \
    src/data/weatherpredictiondatasource.cpp \
    src/data/memorymanageddatasource.cpp \
    src/data/lrumemorymanager.cpp \
    src/data/datarequest.cpp \
    src/data/floatpertrajectorysource.cpp \
    src/data/scheduleddatasource.cpp \
    src/data/scheduler.cpp \
    src/data/task.cpp \
    src/data/trajectoryfilter.cpp \
    src/data/trajectorynormalssource.cpp \
    src/data/pressuretimetrajectoryfilter.cpp \
    src/data/deltapressurepertrajectory.cpp \
    src/data/singletimetrajectoryfilter.cpp \
    src/data/trajectoryselectionsource.cpp \
    src/gxfw/gl/abstractgpudataitem.cpp \
    src/data/probdftrajectoriessource.cpp \
    src/data/climateforecastreader.cpp \
    src/data/weatherpredictionreader.cpp \
    src/gxfw/nwpmultivaractor.cpp \
    src/gxfw/nwpactorvariable.cpp \
    src/gxfw/selectdatasourcedialog.cpp \
    src/data/structuredgridensemblefilter.cpp \
    src/data/verticalregridder.cpp \
    src/data/thinouttrajectoryfilter.cpp \
    src/data/probabilityregiondetector.cpp \
    src/gxfw/nwpactorvariableproperties.cpp \
    src/data/valueextractionanalysis.cpp \
    src/data/abstractanalysis.cpp \
    src/data/regioncontributionanalysis.cpp \
    src/util/metroutines.cpp \
    src/gxfw/gl/shaderuniforms.cpp \
    src/gxfw/gl/shadereffect.cpp \
    src/gxfw/gl/uniform.cpp \
    src/data/gribreader.cpp \
    src/system/applicationconfiguration.cpp \
    src/gxfw/actorcreationdialog.cpp \
    src/gxfw/scenemanagementdialog.cpp \
    src/gxfw/colourmap.cpp \
    src/data/naturalearthdataloader.cpp \
    src/gxfw/transferfunction.cpp \
    src/gxfw/camera.cpp \
    src/gxfw/textmanager.cpp \
    src/gxfw/synccontrol.cpp \
    src/util/mstopwatch.cpp \
    src/actors/basemapactor.cpp \
    src/actors/volumebboxactor.cpp \
    src/actors/trajectoryactor.cpp \
    src/actors/nwphorizontalsectionactor.cpp \
    src/actors/nwpverticalsectionactor.cpp \
    src/actors/nwpsurfacetopographyactor.cpp \
    src/actors/nwpvolumeraycasteractor.cpp \
    src/system/qtproperties.cpp \
    src/actors/movablepoleactor.cpp \
    src/qt_extensions/qtpropertymanager_extensions.cpp \
    src/system/frontendconfiguration.cpp \
    src/system/pipelineconfiguration.cpp \
    src/data/probabltrajectoriessource.cpp \
    src/gxfw/gl/shaderstoragebufferobject.cpp \
    src/gxfw/memberselectiondialog.cpp \
    src/gxfw/adddatasetdialog.cpp \
    src/data/derivedmetvarsdatasource.cpp \
    src/data/bboxtrajectoryfilter.cpp \
    src/gxfw/mresizewindowdialog.cpp \
    src/actors/spatial1dtransferfunction.cpp \
    src/actors/transferfunctioneditor/transferfunctioneditor.cpp \
    src/actors/transferfunctioneditor/colourpicker.cpp \
    src/actors/transferfunctioneditor/colour.cpp \
    src/actors/transferfunctioneditor/editortransferfunction.cpp \
    src/gxfw/rotatedgridsupportingactor.cpp \
    src/gxfw/boundingbox/bboxdockwidget.cpp \
    src/gxfw/boundingbox/boundingbox.cpp \
    src/gxfw/sessionmanagerdialog.cpp \
    src/data/singlevariableanalysis.cpp \
    src/data/structuredgridstatisticsanalysis.cpp \
    src/qt_extensions/scientificdoublespinbox.cpp

FORMS += \
    src/mainwindow.ui \
    src/gxfw/mscenecontrol.ui \
    src/gxfw/msystemcontrol.ui \
    src/data/waypoints/waypointstableview.ui \
    src/gxfw/selectdatasourcedialog.ui \
    src/gxfw/actorcreationdialog.ui \
    src/gxfw/scenemanagementdialog.ui \
    src/gxfw/synccontrol.ui \
    src/gxfw/memberselectiondialog.ui \
    src/gxfw/adddatasetdialog.ui \
    src/gxfw/resizewindowdialog.ui \
    src/gxfw/boundingbox/bboxdockwidget.ui \
    src/gxfw/sessionmanagerdialog.ui

OTHER_FILES += \
    data/log4cplus.properties \
    README \
    INSTALL \
    COPYING \
    src/glsl/hsec_filledcontours.fx.glsl \
    src/glsl/hsec_pseudocolour.fx.glsl \
    src/glsl/effect_template.fx.glsl \
    src/glsl/hsec_verticalinterpolation.fx.glsl \
    src/glsl/simple_coloured_geometry.fx.glsl \
    src/glsl/trajectory_positions.fx.glsl \
    src/glsl/volume_render_utils.glsl \
    src/glsl/volume_sample_utils.glsl \
    src/glsl/singlepassraycaster_ml.fx.glsl \
    src/glsl/compute_minmax_map.fx.glsl \
    src/glsl/trajectory_tubes_shadow.fx.glsl \
    src/glsl/trajectory_positions_shadow.fx.glsl \
    src/glsl/trajectory_tubes.fx.glsl \
    src/glsl/volume_bitfield_utils.glsl \
    src/glsl/hsec_windbarbs.fx.glsl \
    src/glsl/volume_sample_shv_utils.glsl \
    met3D_inclib.pri.template \
    src/glsl/basemap.fx.glsl \
    src/glsl/volume_pressure_utils.glsl \
    src/glsl/volume_hybrid_utils.glsl \
    src/glsl/volume_global_structs_utils.glsl \
    src/glsl/volume_defines.glsl \
    config/default_pipeline.cfg.template \
    config/default_frontend.cfg.template \
    src/glsl/surface_topography.fx.glsl \
    src/glsl/colourbar.fx.glsl \
    src/glsl/vsec_interpolation_filledcontours.fx.glsl \
    src/glsl/vsec_pressureisolines.fx.glsl \
    src/glsl/text.fx.glsl \
    src/glsl/volume_bitfield_raycaster.fx.glsl \
    src/glsl/volume_compute_normalcurves.fx.glsl \
    src/glsl/volume_image.fx.glsl \
    src/glsl/volume_normalcurves_geometry.fx.glsl \
    src/glsl/volume_raycaster.fx.glsl \
    src/glsl/hsec_marching_squares.fx.glsl \
    src/glsl/vsec_marching_squares.fx.glsl \
    src/glsl/volume_normalcurves_initpoints.fx.glsl \
    src/glsl/hsec_texturedcontours.fx.glsl \
    config/cf_stdnames.dat \
    config/log4cplus.properties \
    src/glsl/north_arrow.fx.glsl \
    config/metview/default_frontend.cfg \
    config/metview/default_pipeline.cfg


DEFINES += \
     ENABLE_MET3D_STOPWATCH \
     ENABLE_HYBRID_PRESSURETEXCOORDTABLE \
     ENABLE_RAYCASTER_ACCELERATION \
     DIRECT_SYNCHRONIZATION \
# Tell the qcustomplot header that it will be used as library.
     QCUSTOMPLOT_USE_LIBRARY \
#     LOG_EVENT_TIMES \
#     CONTINUOUS_GL_UPDATE \
#     DEBUG \
#     DEBUG_OUTPUT_MULTITHREAD_SCHEDULER \
#     DEBUG_OUTPUT_MEMORYMANAGER \
#     NETCDF_CF_TEST_ATTRIBUTES

# Link with debug version of qcustomplot if compiling in debug mode,
# else with release library:
CONFIG(debug, release|debug) {
  win32:QCPLIB = qcustomplotd1
  else: QCPLIB = qcustomplotd
} else {
  win32:QCPLIB = qcustomplot1
  else: QCPLIB = qcustomplot
}

# Include project C++ includes and libraries. The met3D_inclib.pri.user
# contains absolute paths specific to the local installation. If you need
# to modify paths, so do in the met3D_inclib.pri.user.
include(met3D_inclib.pri.user)

QMAKE_CXXFLAGS += \
# Enable the C++11 standard support (e.g. extended initialiser lists).
    -std=c++11
