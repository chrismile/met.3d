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
#ifndef MAPPLICATIONCONFIGURATION_H
#define MAPPLICATIONCONFIGURATION_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "data/structuredgrid.h"


namespace Met3D
{

/**
  @brief Base class for configurations that need to be loaded on system start.
  */
class MAbstractApplicationConfiguration
{
public:
    MAbstractApplicationConfiguration() { }
    virtual ~MAbstractApplicationConfiguration() { }

    /**
      Override this method in derived classes to initialize whatever needs
      to be initialized.
     */
    virtual void configure() = 0;
};


/**
  @brief MApplicationConfigurationManager manages the global system
  configuration. One instance of this class is created in @ref MMainWindow;
  Met.3D is configured from the @ref MMainWindow constructor by a call
  to @ref loadConfiguration().
  */
class MApplicationConfigurationManager
{
public:
    MApplicationConfigurationManager();
    virtual ~MApplicationConfigurationManager();

    /**
      Loads the system configuration (usually from file).
     */
    void loadConfiguration();

protected:
    /**
      Registers available actor factories (for actors that can be created
      at runtime).

      If you implement a new actor for Met.3D, add its factory to this
      method.
     */
    void registerActorFactories();

    /**
      Registers instances of @ref MAbstractApplicationConfiguration that
      need to be loaded from @ref loadConfiguration().
     */
    void registerApplicationConfigurations();

    QList<MAbstractApplicationConfiguration*> appConfigs;
};

} // namespace Met3D

#endif // MAPPLICATIONCONFIGURATION_H
