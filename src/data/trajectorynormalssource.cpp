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
#include "trajectorynormalssource.h"

// standard library imports
#include "assert.h"

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"
#include "gxfw/msceneviewglwidget.h"

using namespace std;


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MTrajectoryNormalsSource::MTrajectoryNormalsSource()
    : MScheduledDataSource(),
      trajectorySource(nullptr)
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

MTrajectoryNormals *MTrajectoryNormalsSource::produceData(
        MDataRequest request)
{
    assert(trajectorySource != nullptr);

    LOG4CPLUS_DEBUG(mlog, "computing world space normals..");

    MDataRequestHelper rh(request);

    QStringList args = rh.value("NORMALS_LOGP_SCALED").split("/");
    double log_pBottom_hPa  = args[0].toDouble();
    double deltaZ_deltaLogP = args[1].toDouble();

    rh.remove("NORMALS_LOGP_SCALED");
    MTrajectories *trajectories = trajectorySource->getData(rh.request());

    MTrajectoryNormals *worldSpaceNormals = new MTrajectoryNormals(
                trajectories->getGeneratingRequest(),
                trajectories->getNumTrajectories(),
                trajectories->getNumTimeStepsPerTrajectory());

    // Loop over all trajectories and compute normals for each of their
    // vertices.
    int numTrajectories = trajectories->getNumTrajectories();
    int numTimeStepsPerTrajectory = trajectories->getNumTimeStepsPerTrajectory();
    QVector<QVector3D> vertices = trajectories->getVertices();

    for (int i = 0; i < numTrajectories; i++)
    {
        int baseIndex = i * numTimeStepsPerTrajectory;

        while (baseIndex+1 < vertices.size())
        {
            QVector3D p0 = vertices.at(baseIndex);
            if (p0.z() == M_INVALID_TRAJECTORY_POS)
            {
                baseIndex++;
            }
            else
            {
                break;
            }
        }

        // Prevent "out of bound exception" ("vertices" are access at
        // "baseIndex+1").
        if (baseIndex+1 >= vertices.size())
        {
            continue;
        }

        // Get the two points of the first line segment; convert pressure to
        // world Z. If p1 (the second point) is invalid, then (a) the normal
        // for the first point cannot be computed, and (b) we assume that the
        // entire trajectory is invalid (it cannot be forward integrated from
        // an invalid position) -- hence continue; the normals will be the
        // default zero normals.
        QVector3D p1 = vertices.at(baseIndex+1);
        if (p1.z() == M_INVALID_TRAJECTORY_POS) continue;
        p1.setZ(MSceneViewGLWidget::worldZfromPressure(
                    p1.z(), log_pBottom_hPa, deltaZ_deltaLogP));
        QVector3D p0 = vertices.at(baseIndex  );
        p0.setZ(MSceneViewGLWidget::worldZfromPressure(
                    p0.z(), log_pBottom_hPa, deltaZ_deltaLogP));

        QVector3D segment = p1-p0;
        segment.normalize();

        // Compute an arbitrary normal on the line segment by taking the cross
        // product with (1,0,0). If the length of the resulting vector is close
        // to zero the segment's orientation was close to (1,0,0) -- use
        // (0,1,0) instead.
        QVector3D normal =
                QVector3D::crossProduct(segment, QVector3D(1., 0., 0.));
        if (normal.length() < 0.01)
            normal = QVector3D::crossProduct(segment, QVector3D(0., 1., 0.));
        normal.normalize();

        worldSpaceNormals->setNormal(baseIndex, normal);

        // For all segments of the trajectory ..
        for (int t = 2; t < numTimeStepsPerTrajectory; t++)
        {
            // .. compute the segment vector ..
            p0 = p1;
            p1 = vertices.at(baseIndex+t);

            if (p1.z() == M_INVALID_TRAJECTORY_POS)
            {
                // If the second point of this segment is invalid but the first
                // point is valid, copy the previous normal (the current index
                // is assumed to be the last valid index of the trajectory).
                // If both are invalid, just continue and leave the normal at
                // its default zero value.
                if (p0.z() != M_INVALID_TRAJECTORY_POS)
                    worldSpaceNormals->setNormal(baseIndex+t-1, normal);
                continue;
            }

            p1.setZ(MSceneViewGLWidget::worldZfromPressure(
                        p1.z(), log_pBottom_hPa, deltaZ_deltaLogP));

            segment = p1-p0;
            segment.normalize();

            //TODO: which is correct? (mr, 18Mar2013)
            //            QVector3D p2 = latLonPVertices.at(baseIndex+t);
            //            p2.setZ(MSceneViewGLWidget::worldZfromPressure(
            //                      p2.z(), log_pBottom_hPa, deltaZ_deltaLogP));

            //            segment = p2-p0;
            //            segment.normalize();

            //            p0 = p1;
            //            p1 = p2;

            // .. compute the binormal, which is perpendicular to both the
            // segment and the previous normal; then compute a vector
            // perpendicular to binormal and segment to "rotate backwards" (cf
            // notes of 12Mar2013).
            QVector3D binormal = QVector3D::crossProduct(segment, normal);
            normal = QVector3D::crossProduct(binormal, segment);
            normal.normalize();

            worldSpaceNormals->setNormal(baseIndex+t-1, normal);
        }

        // The last segment gets the last computed normal twice.
        worldSpaceNormals->setNormal(baseIndex+numTimeStepsPerTrajectory-1,
                                     normal);
    }

    trajectorySource->releaseData(trajectories);
    LOG4CPLUS_DEBUG(mlog, ".. world space normals done.");
    return worldSpaceNormals;
}


MTask* MTrajectoryNormalsSource::createTaskGraph(MDataRequest request)
{
    assert(trajectorySource != nullptr);

    MTask *task = new MTask(request, this);

    // Add dependency: the trajectories.
    MDataRequestHelper rh(request);
    rh.remove("NORMALS_LOGP_SCALED");
    task->addParent(trajectorySource->getTaskGraph(rh.request()));

    return task;
}


void MTrajectoryNormalsSource::setTrajectorySource(MTrajectoryDataSource* s)
{
    trajectorySource = s;
    registerInputSource(trajectorySource);
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/

const QStringList MTrajectoryNormalsSource::locallyRequiredKeys()
{
    return (QStringList() << "NORMALS_LOGP_SCALED");
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/



} // namespace Met3D
