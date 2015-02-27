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
#include "valueextractionanalysis.h"

// standard library imports
#include <iostream>
#include "assert.h"

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "gxfw/nwpmultivaractor.h"

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     MValueExtractionAnalysis                            ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MValueExtractionAnalysis::MValueExtractionAnalysis()
    : MAnalysisDataSource()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

MAnalysisResult* MValueExtractionAnalysis::produceData(MDataRequest request)
{
    MAnalysisResult *result = new MAnalysisResult();

    MDataRequestHelper rh(request);
    QVector3D pos_lonLatP = rh.vec3Value("POS_LONLATP");
    QStringList referencedDatasources =
            rh.value("REFERENCED_DATASOURCES").split("/");

    for (int i = 0; i < referencedDatasources.size(); i++)
    {
        // Extract subrequest for actor variable i.
        QString prefix = referencedDatasources[i];

        // Obtain pointer to data source. getPrefixedDataSource() is
        // thread-safe.
        MWeatherPredictionDataSource *source =
                static_cast<MWeatherPredictionDataSource*>(
                    getPrefixedDataSource(prefix));

        if (source == nullptr)
        {
            LOG4CPLUS_ERROR(mlog, "ERROR: Request references unavailable "
                            "datasource: " << prefix.toStdString());

            // As this analysis module only prints the value of the data field
            // at the specified position its execution won't fail if we simply
            // ignore the missing data source. Other modules might raise an
            // exception here.
            continue;
        }

        MDataRequestHelper varRH = rh.subRequest(prefix);

        MStructuredGrid *grid = source->getData(varRH.request());

        QString analysis =
                QString("data source \"%1\", var\"%2\" at (%3/%4/%5): %6")
                .arg(prefix).arg(grid->getVariableName())
                .arg(pos_lonLatP.x()).arg(pos_lonLatP.y()).arg(pos_lonLatP.z())
                .arg(grid->interpolateValue(pos_lonLatP));

        result->textResult.append(analysis);

        MIndex3D nw_top, ne_top, sw_top, se_top;
        bool insideGridVolume = grid->findTopGridIndices(pos_lonLatP, &nw_top,
                                                         &ne_top, &sw_top, &se_top);

        if (insideGridVolume)
        {
            MIndex3D nw_bot = nw_top; nw_bot.k += 1;
            result->textResult.append(
                        QString("  north-west grid column: %1=%2, %3=%4")
                        .arg(nw_top.toString()).arg(grid->getValue(nw_top))
                        .arg(nw_bot.toString()).arg(grid->getValue(nw_bot)));

            MIndex3D ne_bot = ne_top; ne_bot.k += 1;
            result->textResult.append(
                        QString("  north-east grid column: %1=%2, %3=%4")
                        .arg(ne_top.toString()).arg(grid->getValue(ne_top))
                        .arg(ne_bot.toString()).arg(grid->getValue(ne_bot)));

            MIndex3D sw_bot = sw_top; sw_bot.k += 1;
            result->textResult.append(
                        QString("  south-west grid column: %1=%2, %3=%4")
                        .arg(sw_top.toString()).arg(grid->getValue(sw_top))
                        .arg(sw_bot.toString()).arg(grid->getValue(sw_bot)));

            MIndex3D se_bot = se_top; se_bot.k += 1;
            result->textResult.append(
                        QString("  south-east grid column: %1=%2, %3=%4")
                        .arg(se_top.toString()).arg(grid->getValue(se_top))
                        .arg(se_bot.toString()).arg(grid->getValue(se_bot)));

            MIndex3D maxgp = grid->maxNeighbouringGridPoint(pos_lonLatP);
            result->textResult.append(
                        QString("  maximum neighbour at grid point: %1=%2")
                        .arg(maxgp.toString()).arg(grid->getValue(maxgp)));
        }

        source->releaseData(grid);
    }

    return result;
}


MTask* MValueExtractionAnalysis::createTaskGraph(MDataRequest request)
{
    MTask *task = new MTask(request, this);

    MDataRequestHelper rh(request);
    QStringList referencedDatasources =
            rh.value("REFERENCED_DATASOURCES").split("/");

    for (int i = 0; i < referencedDatasources.size(); i++)
    {
        // Extract subrequest for actor variable i.
        QString prefix = referencedDatasources[i];
        MDataRequestHelper varRH = rh.subRequest(prefix);

        MWeatherPredictionDataSource *source =
                static_cast<MWeatherPredictionDataSource*>(
                    getPrefixedDataSource(prefix));

        task->addParent(source-> getTaskGraph(varRH.request()));
    }

    return task;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MValueExtractionAnalysis::locallyRequiredKeys()
{
    return QStringList() << "POS_LONLATP" << "REFERENCED_DATASOURCES";
}




/******************************************************************************
***                  MValueExtractionAnalysisControl                        ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MValueExtractionAnalysisControl::MValueExtractionAnalysisControl(
        MNWPMultiVarActor *actor)
    : MAnalysisControl(actor)
{
    resultsTextBrowser = new QTextBrowser();
    resultsTextBrowser->setLineWrapMode(QTextBrowser::NoWrap);
    setDisplayWidget(resultsTextBrowser);
    setDisplayTitle("Value Extraction Analysis");
}


MValueExtractionAnalysisControl::~MValueExtractionAnalysisControl()
{
    delete resultsTextBrowser;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MValueExtractionAnalysisControl::displayResult(MAnalysisResult *result)
{
    //LOG4CPLUS_DEBUG(mlog, "result of value extraction analysis:");

    QString s;
    for (int i = 0; i < result->textResult.size(); i++)
    {
        //LOG4CPLUS_DEBUG(mlog, result->textResult[i].toStdString());
        s += result->textResult[i] + "\n";
    }

    resultsTextBrowser->setPlainText(s);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

MDataRequest MValueExtractionAnalysisControl::prepareRequest(
        MDataRequest analysisRequest)
{
    MDataRequestHelper rh(analysisRequest);

    // This analysis filter requires the data fields that are currently
    // kept by the actor's MNWPActorVariables.
    int numActorVars = actor->getNWPVariables().size();
    QString referencedDatasources = "";

    for (int i = 0; i < numActorVars; i++)
    {
        MNWPActorVariable* var = actor->getNWPVariables()[i];
        MDataRequestHelper gridRH(var->grid->getGeneratingRequest());
        QString prefix = QString("%1_").arg(i);
        referencedDatasources += prefix + "/";
        gridRH.addKeyPrefix(prefix);
        rh.unite(gridRH);
    }

    referencedDatasources.chop(1); // remove trailing "/"
    rh.insert("REFERENCED_DATASOURCES", referencedDatasources);

    return rh.request();
}


void MValueExtractionAnalysisControl::updateAnalysisSourceInputs()
{
    analysisSource->clearDataSources();

    for (int i = 0; i < actor->getNWPVariables().size(); i++)
        analysisSource->addDataSource(QString("%1_").arg(i),
                                      actor->getNWPVariables()[i]->dataSource);
}


} // namespace Met3D
