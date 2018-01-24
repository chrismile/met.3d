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
#include "abstractanalysis.h"

// standard library imports
#include <iostream>
#include "assert.h"

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "gxfw/msystemcontrol.h"
#include "mainwindow.h"
#include "gxfw/nwpmultivaractor.h"

using namespace std;

namespace Met3D
{

unsigned int MAnalysisResult::getMemorySize_kb()
{
    int textSize = 0;
    for (int i = 0; i < textResult.size(); i++)
        textSize += sizeof(textResult[i]);

    return ( sizeof(MAnalysisResult)
             + textSize
             ) / 1024.;
}


/******************************************************************************
***                       MAnalysisDataSource                               ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MAnalysisDataSource::MAnalysisDataSource()
    : MScheduledDataSource()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MAnalysisDataSource::addDataSource(
        QString prefix, MWeatherPredictionDataSource *ds)
{
    registerInputSource(ds, prefix);
}


void MAnalysisDataSource::clearDataSources()
{
    deregisterPrefixedInputSources();
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/





/******************************************************************************
***                          MAnalysisControl                               ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MAnalysisControl::MAnalysisControl(MNWPMultiVarActor *actor)
    : QObject(),
      actor(actor),
      analysisSource(nullptr)
{
    actor->setAnalysisControl(this);

    dock = new QDockWidget("Analysis",
                           MSystemManagerAndControl::getInstance()->getMainWindow());
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    // dock->setFloating(true);
    dock->setVisible(false);

    MSystemManagerAndControl::getInstance()->getMainWindow()->addDockWidget(
                Qt::LeftDockWidgetArea, dock);
}


MAnalysisControl::~MAnalysisControl()
{
    if (analysisSource)
    {
        delete analysisSource;
    }

    if (dock)
    {
        delete dock;
    }
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MAnalysisControl::run(MDataRequest analysisRequest)
{
    if (analysisSource == nullptr)
    {
        analysisSource = createAnalysisSource();
        analysisSource->setMemoryManager(memoryManager);
        analysisSource->setScheduler(scheduler);
        connect(analysisSource, SIGNAL(dataRequestCompleted(MDataRequest)),
                this, SLOT(requestCompleted(MDataRequest)));
    }

//TODO: Inefficient. Better solution to check if the actor's data sources have
//      changed since the last call to run()? (mr, Apr2014)
    updateAnalysisSourceInputs();

    MDataRequest dataRequest = prepareRequest(analysisRequest);   

    // Place the requests into the QSet pendingRequests to decide in O(1)
    // in asynchronousDataAvailable() whether to accept an incoming request.
    pendingRequests.insert(dataRequest);
    // Place the request into the request queue to ensure correct order
    // when incoming requests are handled.
    MRequestQueueInfo rqi;
    rqi.request = dataRequest;
    rqi.available = false;
    pendingRequestsQueue.enqueue(rqi);

    analysisSource->requestData(dataRequest);
}


/******************************************************************************
***                             PUBLIC SLOTS                                ***
*******************************************************************************/

void MAnalysisControl::requestCompleted(MDataRequest request)
{
    // Decide in O(1) based on the QSet whether to accept the incoming request.
    if (!pendingRequests.contains(request)) return;
    pendingRequests.remove(request);

    // Mark the incoming request as "available" in the request queue. Usually
    // the requests are received in correct order, i.e. this loop on average
    // will only compare the first entry.
    for (int i = 0; i < pendingRequestsQueue.size(); i++)
    {
        if (pendingRequestsQueue[i].request == request)
        {
            pendingRequestsQueue[i].available = true;
            break;
        }
    }

    // Display analysis results as long as they are available in the order in
    // which they were requested.
    while ( ( !pendingRequestsQueue.isEmpty() ) &&
            pendingRequestsQueue.head().available )
    {
        MRequestQueueInfo rqi = pendingRequestsQueue.dequeue();
        MDataRequest processRequest = rqi.request;

        MAnalysisResult *result = analysisSource->getData(processRequest);
        displayResult(result);
        dock->show();
        analysisSource->releaseData(result);
    }
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

void MAnalysisControl::setDisplayWidget(QWidget *widget)
{
    dock->setWidget(widget);
}


void MAnalysisControl::setDisplayTitle(const QString& title)
{
    dock->setWindowTitle(title);
}


} // namespace Met3D
