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
#include "abstractdatareader.h"

// standard library imports

// related third party imports

// local application imports

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

QMutex MAbstractDataReader::staticNetCDFAccessMutex;

MAbstractDataReader::MAbstractDataReader(QString identifier)
    : identifier(identifier)
{
}


MAbstractDataReader::~MAbstractDataReader()
{
    // Clear file scan progress dialog list. (This should not be necessary if
    // file scan progress dialogs are deleted correctly.)
    while (!fileScanProgressDialogList.isEmpty())
    {
        deleteFileScanProgressDialog();
    }
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MAbstractDataReader::setDataRoot(QString path, QString fFilter)
{
    dataRoot = QDir(path);
    dirFileFilters = fFilter;
    scanDataRoot();
}


void MAbstractDataReader::getAvailableFilesFromFilters(QStringList &availableFiles)
{
    availableFiles.clear();
    QString rootPath = dataRoot.absolutePath();
    QString filters = dirFileFilters;
    filters.replace("%m", "*");
    QStringList dirFilters = filters.split("/", QString::SkipEmptyParts);
    // Take the last part of the filters list as file filter.
    QString fileFilter = dirFilters.takeLast();

    if (!dirFilters.isEmpty())
    {
        // Get directories.

        // Initialise availableFile with the first "sub-directory-layer".
        availableFiles = dataRoot.entryList(
                    QStringList(dirFilters.takeFirst()),
                    QDir::Dirs | QDir::NoDotAndDotDot);
        int numPaths = availableFiles.size();
        foreach (QString dirFilter, dirFilters)
        {
            for (int i = 0; i < numPaths; ++i)
            {
                // Set directory path to the path to the current sub-directory
                // to be searched in and filter the sub-directories according to
                // the filter of the  current "layer".
                QString path = availableFiles.takeFirst();
                dataRoot.setPath(rootPath + "/" + path);
                QStringList availableDirs = dataRoot.entryList(
                            QStringList(dirFilter),
                            (QDir::Dirs | QDir::NoDotAndDotDot));
                foreach (QString availableDir, availableDirs)
                {
                    availableFiles.append(path + "/" + availableDir);
                }
            }
            numPaths = availableFiles.size();
        }

        // Get files.
        for (int i = 0; i < numPaths; ++i)
        {
            QString path = availableFiles.takeFirst();
            dataRoot.setPath(rootPath + "/" + path);
            QStringList availableFileList = dataRoot.entryList(
                        QStringList(fileFilter), QDir::Files);
            foreach (QString availableFile, availableFileList)
            {
                availableFiles.append(path + "/" + availableFile);
            }
        }
    }
    else
    {
        // No directory filters given thus only search for files.
        availableFiles = dataRoot.entryList(
                    QStringList(fileFilter), QDir::Files);
    }

    // Since dataRoot was used to search the sub-directories, reset dataRoot to
    // the root directory.
    dataRoot.setPath(rootPath);
}


int MAbstractDataReader::getEnsembleMemberIDFromFileName(QString fileName)
{
    QString ensembleNameFilter = QRegExp::escape(dirFileFilters);
    ensembleNameFilter.replace("\\*", ".*");
    ensembleNameFilter.replace("%m", "(\\d+)");

    QRegExp findEnsemble(ensembleNameFilter);

    findEnsemble.exactMatch(fileName);
    // Get ensemble member.
    QString ensembleMember = findEnsemble.cap(1);

    if (ensembleMember.isEmpty())
    {
        return -1;
    }

    return ensembleMember.toInt();
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MAbstractDataReader::initializeFileScanProgressDialog(
        int numFiles, QString labelText)
{
    if (labelText.isEmpty())
    {
        labelText = "Loading data set " + getIdentifier() + " ...";
    }
    fileScanProgressDialogList.append(new QProgressDialog(
                labelText, "cancel",
                0, numFiles));
    // Hide close, resize and minimize buttons. (Use tool tip since dialog won't
    // be shown right away).
    fileScanProgressDialogList.last()->setWindowFlags(
                Qt::ToolTip | Qt::CustomizeWindowHint);
    fileScanProgressDialogList.last()->setMinimumDuration(0);
    fileScanProgressDialogList.last()->setCancelButton(0);
    // Always show progress dialog right away.
    fileScanProgressDialogList.last()->setVisible(true);

    loadingProgressList.append(0);
    fileScanProgressDialogList.last()->setValue(0);
    fileScanProgressDialogList.last()->update();
    qApp->processEvents();
}


void MAbstractDataReader::updateFileScanProgressDialog()
{
    fileScanProgressDialogList.last()->setValue(
                ++loadingProgressList[loadingProgressList.size() - 1]);
    fileScanProgressDialogList.last()->update();
    qApp->processEvents();
}


void MAbstractDataReader::deleteFileScanProgressDialog()
{
    if (fileScanProgressDialogList.last() != nullptr)
    {
        delete fileScanProgressDialogList.takeLast();
        loadingProgressList.removeLast();
    }
}

} // namespace Met3D
