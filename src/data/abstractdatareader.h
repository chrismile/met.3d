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
#include <QProgressDialog>

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
    void setDataRoot(QString path, QString fFilter);

    /**
      Gets file paths relative to @ref dataRoot fulfilling the restrictions of
      @ref dirFileFilters.

      The file paths are stored to @p availableFiles.

      For searching a filter is used with the ensemble member identifier place
      holder "%m" replaced by "*".
     */
    void getAvailableFilesFromFilters(QStringList &availableFiles);

    /**
      For ensemble datasets that do NOT store ensemble dimension in their
      NetCDF/GRIB files but instead have the ensemble member ID encoded in
      their file or subdirectory names (e.g., "my_forecast.004.grb" or
      "member_004/temperature.nc"), gets the ensemble member identifier form @p
      fileName by applying @ref dirFileFilters (the position of the ensemble ID
      is specified by "%m" placed in the file filter string. Example:
      "my_forecast.%m.grb" or "member_%m/temperature.nc").

      For searching, a regular expression is built from @ref dirFileFilters in
      which the ensemble member identifier place holder "%m" is replaced by
      "\d+".

      @return the ensemble member identifier if found and -1 otherwise.

      Note: At the moment Met.3D only allows positive integers as ensemble
      identifiers (leading zeros allowed).
     */
    int getEnsembleMemberIDFromFileName(QString fileName);

protected:
    /**
     Scans the data root directory to determine which data is available. This
     method is called from @ref setDataRoot() and needs to be implemented in
     derived classes.
     */
    virtual void scanDataRoot() = 0;

    /**
      Creates a progress dialog which can be used to monitor the progress while
      reading data set files and appends it to @ref fileScanProgressDialogList.

      If @p labelText is empty, ("Loading data set " + getIdentifier() + " ...")
      is used as default label text.

      Call @ref updateFileScanProgressDialog() to update progress bar.

      After using it call @ref deleteFileScanProgressDialog() to delete the
      progressbar.
     */
    void initializeFileScanProgressDialog(int numFiles, QString labelText = "");

    /**
      Updates last entry in @p fileScanProgressDialogList and @ref
      loadingProgressList by inceasing the progress by one.

      @note A progress dialog needs to be initialized by calling @ref
      initializeFileScanProgressDialog() before using this method!
     */
    void updateFileScanProgressDialog();

    /**
      Deletes last entry in @p fileScanProgressDialogList and removes last
      entry in @ref fileScanProgressDialogList and @ref loadingProgressList.
     */
    void deleteFileScanProgressDialog();

    QString identifier;
    QDir dataRoot;
    QString dirFileFilters;

    /** Global NetCDF access mutex, as the NetCDF (C++) library is not
        thread-safe (notes Feb2015). This mutex must be used to protect
        ALL access to NetCDF files in Met.3D! */
    static QMutex staticNetCDFAccessMutex;

    QList<QProgressDialog*> fileScanProgressDialogList;
    QList<int> loadingProgressList;
};


} // namespace Met3D

#endif // ABSTRACTDATAREADER_H
