/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2019 Marc Rautenhaus
**
**  Regional Computing Center, Visualization
**  Universitaet Hamburg, Hamburg, Germany
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
#ifndef MDEVELOPMENTAIDSCONFIGURATION_H
#define MDEVELOPMENTAIDSCONFIGURATION_H

// standard library imports

// related third party imports

// local application imports
#include "system/applicationconfiguration.h"


namespace Met3D
{

/**
  @brief MDevelopmentAidsConfiguration initializes modules that aid Met.3D
  development, e.g. automatic tests.
  */
class MDevelopmentAidsConfiguration : public MAbstractApplicationConfiguration
{
public:
    MDevelopmentAidsConfiguration();

    void configure();

protected:
    void runSimpleTests();

};

} // namespace Met3D

#endif // MDEVELOPMENTAIDSCONFIGURATION_H
