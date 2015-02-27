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
#ifndef ABSTRACTDATAREADER_H
#define ABSTRACTDATAREADER_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports


namespace Met3D
{

/**
  @brief MAbstractDataReader is the base class for all data reader
  implementations.
  */
class MAbstractDataReader
{
public:
    MAbstractDataReader(QString identifier);
    virtual ~MAbstractDataReader();

    /**
      Returns the identifier string of this data loader.
      */
    QString getIdentifier() { return identifier; }

    /**
      Sets @p path to be the root directory in which the data files for this
      data loader's dataset are located. @p path is scanned and the available
      variables can afterwards be accessed through ..
      */
    void setDataRoot(QString path);

protected:
    /**
     Scans the data root directory to determine which data is available. This
     method is called from @ref setDataRoot() and needs to be implemented in
     derived classes.
     */
    virtual void scanDataRoot() = 0;

    QString identifier;
    QDir dataRoot;

    /** Global NetCDF access mutex, as the NetCDF (C++) library is not
        thread-safe (notes Feb2015). This mutex must be used to protect
        ALL access to NetCDF files in Met.3D! */
    static QMutex staticNetCDFAccessMutex;
};


} // namespace Met3D

#endif // ABSTRACTDATAREADER_H
