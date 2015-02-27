/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
**
**  Computer Graphics and Visualization Group
**  Technische Universitaet Muenchen, Garching, Germany
**
**  Met.3D is free software: you can redistribute it and/or modify
**  it under the terms of the GNU General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  Met.3D is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU General Public License for more details.
**
**  You should have received a copy of the GNU General Public License
**  along with Met.3D.  If not, see <http://www.gnu.org/licenses/>.
**
*******************************************************************************/

// standard library imports

// related third party imports
#include <QApplication>
#include <log4cplus/configurator.h>
#include <log4cplus/loggingmacros.h>

// local application imports
#include "mainwindow.h"
#include "util/mutil.h"


int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QStringList commandLineArguments = app.arguments();

    // Initialise the log4cplus logging mechanism.
    // See http://log4cplus.sourceforge.net/index.html.
    log4cplus::PropertyConfigurator::doConfigure(
                LOG4CPLUS_TEXT("config/log4cplus.properties"));

    LOG4CPLUS_INFO(mlog, "===================================================="
                   "============================");
    LOG4CPLUS_INFO(mlog, "Met.3D -- interactive 3D visualization of numerical "
                   "ensemble weather predictions");
    LOG4CPLUS_INFO(mlog, "===================================================="
                   "============================");
    LOG4CPLUS_INFO(mlog, "");
    LOG4CPLUS_INFO(mlog, "Met.3D is free software under the GNU General "
                   "Public License.");
    LOG4CPLUS_INFO(mlog, "It is distributed in the hope that it will be useful, "
                   "but WITHOUT ANY WARRANTY.");
    LOG4CPLUS_INFO(mlog, "");

    // Check for OpenGL.
    if (!QGLFormat::hasOpenGL())
    {
        LOG4CPLUS_ERROR(mlog, "ERROR: This system has no OpenGL support.");
        return 1;
    }

    // Create the application window and run the application.
    QString applicationTitle = QString(
                "Met.3D  (built %1)").arg(__DATE__);

    Met3D::MMainWindow win(commandLineArguments);
    win.setWindowTitle(applicationTitle);
//    win.resize(1280, 610);
    win.resize(1288, 610);
//    win.resize(1600, 900);
//    win.resize(1800, 800);
    win.show();

    return app.exec();
}

/**
  \mainpage

  \section sect_intro Introduction

  Met.3D is a a research environment for the three-dimensional visual
  exploration of numerical ensemble weather prediction data (ensemble NWP
  data).

  This page is intended as an entry point to the source code documentation.

  \section sect_code Code structure

  The code is structured in several directories:
    - config/
      - contains configuration files
    - doc/
      - contains configuration scripts for the doxygen documentation you are
        currently reading
    - src/
      - actors/
        - contains code of actors, i.e. classes that implement a visual entity
          within a scene
      - data/
        - contains classes that handle data I/O, e.g. data readers that read
          numerical weather prediction data
      - glsl/
        - contains GLSL shaders
      - gxfw/
        - contains classes that contribute to the graphics framework of the
          system, e.g. scene controls, camera, text manager, etc.
      - qt_extensions/
        - contains extensions to the Qt library
      - system/
        - system configuration
      - util/
        - general utility code, e.g. error handling


  \section sect_impl Implementation

  \subsection ssect_actors Scenes and Actors

  The major visualization entities are \em scenes and \em actors. An actor
  implements a specific visualization element, for instance, a horizontal 2D
  section or a colour bar. A scene consists of a group of actors that are
  displayed together in one view. An actor can be displayed in multiple scenes.

  A scene is rendered by a \em scene \em view. A scene view implements an
  OpenGL context. Multiple scene views can be displayed in the application
  window, so that the user can simultaneously view different scenes. Also, the
  same scene can be rendered by different scene views to allow for different
  viewing angles.

  Actors are derived from \ref Met3D::MActor. For example, to visualize NWP
  data, the class \ref Met3D::MNWPMultiVarActor provides common functionality
  to load multiple NWP data fields via a data loader. \ref
  Met3D::MNWPHorizontalSectionActor in turn inherits from
  Met3D::MNWPMultiVarActor and implements the code necessary to render
  horizontal 2D sections.

  Scenes are implemented in \ref Met3D::MSceneControl. Met3D::MSceneControl
  also provides a GUI element that allows the use to adjust properties
  associated with the scene and its actors. Properties are presented to the
  used using a tree view provided by the QtPropertyBrowser
  (http://doc.qt.digia.com/solutions/4/qtpropertybrowser/index.html). This way,
  actors only need to define the properties they provide and not care about GUI
  elements.

  \ref Met3D::MSceneViewGLWidget implements the scene view. When multiple scene
  views are present in the application, each owns a separate OpenGL context. To
  share resources between the views, a hidden OpenGL context, the \ref
  Met3D::MGLResourcesManager, owns all sharable textures, vertex buffers etc.

  \subsection ssect_controls Controls

  \em Controls are modules that provide a GUI module that lets the user control
  specific functionality of the program. The scene control was already
  mentioned above. The \ref Met3D::MSystemManagerAndControl provides control
  over global system properties, the \ref Met3D::MSyncControl provides time and
  ensemble settings for global synchronization.

  \subsection ssect_dataloaders Data Pipeline

  Actors obtain their data fields from \em data \em sources derived from \ref
  Met3D::MAbstractDataSource. A data source can be a reader that reads a data
  field from disk (for example, \ref Met3D::MECMWFClimateForecastReader), or a
  module that computes a new data field based on input data (for example, the
  ensemble mean is computed from the individual ensemble members in \ref
  Met3D::MStructuredGridEnsembleFilter). Data sources can be put together to
  form a \em pipeline. The actor is connected to the last data source in the
  pipeline. When it sends a \em request into the pipeline that request
  propagetes down the data sources and each source handles the part of the
  request it is responsible for. When the request is completed the resulted
  data field is passed to the actor.

  \subsection ssect_text Text Rendering

  Text rendering is implemented by means of a texture atlas in the class \ref
  Met3D::MTextManager.

  \subsection ssect_waypoints Waypoints and Vertical Sections

  The application provides a data structure that stores a list of waypoints,
  the \ref Met3D::MWaypointsTableModel. The waypoints are used, for instance,
  for the vertical cross sections in \ref Met3D::MNWPVerticalSectionActor to
  allow for vertical cross sections along arbitrary paths. They can also
  represent a flight track that is planned with the system. The implementation
  is similar to that of the "Mission Support System" (MSS,
  http://www.geosci-model-dev.net/5/55/2012/ , see the Supplement), flight
  tracks can be opened and saved in both systems.

  \section sect_lic Met.3D License

  Copyright 2015 Marc Rautenhaus

  Computer Graphics and Visualization Group,
  Technische Universitaet Muenchen, Garching, Germany

  Met.3D is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Met.3D is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Met.3D.  If not, see <http://www.gnu.org/licenses/>.

*/
