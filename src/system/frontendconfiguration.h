/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2015-2017 Bianca Tost
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
#ifndef MDEFAULTFRONTENDCONFIGURATION_H
#define MDEFAULTFRONTENDCONFIGURATION_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "system/applicationconfiguration.h"
#include "data/structuredgrid.h"


namespace Met3D
{

/**
  @brief MFrontendConfiguration initializes Met.3D frontend modules. This
  includes synchronization control, waypoints models and, of course, actors.
  The class contains a number of pre-defined actor configurations that can be
  loaded on system start (methods "initializeDefaultActors_..."). The more
  common actor intiailization mechanism, however, is that users save an actor
  configuration into an actor config file and reference this file in the
  frontend condfiguration file.
  */
class MFrontendConfiguration : public MAbstractApplicationConfiguration
{
public:
    MFrontendConfiguration();

    void configure();

protected:
    /**
      Configures the system with the settings stored in file @p filename.
     */
    void initializeFrontendFromConfigFile(const QString& filename);

    void initializeSynchronization(QString syncName,
            QStringList initializeFromDataSources);

    void initializeDefaultActors_Basemap(
            const QString& mapfile,
            const QRectF &bbox,
            QList<MSceneControl*> scenes);

    void initializeDefaultActors_VolumeBox(
            const QRectF &bbox,
            QList<MSceneControl*> scenes);

    void initializeDefaultActors_MSLP(
            const QString &dataSourceID,
            const QRectF &bbox,
            QList<MSceneControl*> scenes);

    void initializeDefaultActors_Surface(
            const QString &dataSourceID,
            const QRectF &bbox,
            QList<MSceneControl*> scenes);

    void initializeDefaultActors_HSec(
            const QString &dataSourceID,
            const QRectF &bbox,
            QList<MSceneControl*> scenes);

    void initializeDefaultActors_HSec_Difference(
            const QString &dataSourceID,
            const QRectF &bbox,
            QList<MSceneControl*> scenes);

    void initializeDefaultActors_VSec_PV(
            const QString &dataSourceID,
            QList<MSceneControl*> scenes);

    void initializeDefaultActors_VSec_Clouds(
            const QString &dataSourceID,
            QList<MSceneControl*> scenes);

    void initializeDefaultActors_Volume(
            const QString &dataSourceID,
            const QRectF &bbox,
            QList<MSceneControl*> scenes);

    void initializeDefaultActors_VolumeProbability(
            const QString &dataSourceID,
            const MVerticalLevelType levelType,
            const QString& nwpDataSourceID,
            const QRectF &bbox,
            QList<MSceneControl*> scenes);

    void initializeDefaultActors_PressurePoles(
            QList<MSceneControl*> scenes);

    void initializeDefaultActors_Trajectories(
            const QString& dataSourceID,
            QList<MSceneControl*> scenes);

    /**
      Hard-coded frontend initialization. Use this method for development
      purposes.

      @see configure()
     */
    void initializeDevelopmentFrontend();
};

} // namespace Met3D

#endif // MDEFAULTFRONTENDCONFIGURATION_H
