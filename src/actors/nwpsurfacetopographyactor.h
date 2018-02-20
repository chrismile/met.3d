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
#ifndef LONLATSURFACEACTOR_H
#define LONLATSURFACEACTOR_H

// standard library imports
#include <memory>

// related third party imports
#include <GL/glew.h>
#include <QtProperty>

// local application imports
#include "gxfw/nwpmultivaractor.h"
#include "gxfw/nwpactorvariable.h"
#include "gxfw/gl/shadereffect.h"
#include "actors/transferfunction1d.h"
#include "actors/graticuleactor.h"

class MSceneViewGLWidget;

namespace Met3D
{

/**
  @brief MNWPSurfaceTopographyActor renders a 2D NWP surface field with
  topography.

  Rendering is implemented using "instanced rendering" (notes 09Feb2012).

  @todo Revise implementation of lighting. (mr, 13Feb2012)
  @todo Make this work with PVU and, in general, ML surfaces! (mr, 13Feb2012)
  */
class MNWPSurfaceTopographyActor : public MNWPMultiVarActor
{
public:
    MNWPSurfaceTopographyActor();
    virtual ~MNWPSurfaceTopographyActor();

    static QString staticActorType() { return "Surface topography"; }

    void reloadShaderEffects() override;

    const QList<MVerticalLevelType> supportedLevelTypes() override;

    MNWPActorVariable *createActorVariable(
            const MSelectableDataSource& dataSource) override;

    QtProperty* topographyVariableIndexProp;

    QtProperty* shadingVariableIndexProp;

    QString getSettingsID() override { return "NWPSurfaceTopographyActor"; }

    void saveConfiguration(QSettings *settings) override;

    void loadConfiguration(QSettings *settings) override;

protected:
    void initializeActorResources() override;

    void onQtPropertyChanged(QtProperty* property) override;

    void renderToCurrentContext(MSceneViewGLWidget *sceneView) override;

    void dataFieldChangedEvent() override;

    void onDeleteActorVariable(MNWPActorVariable* var) override;

    void onAddActorVariable(MNWPActorVariable* var) override;

    uint8_t topographyVariableIndex;
    uint8_t shadingVariableIndex;

    QList<QString> varNames;

private:
    std::shared_ptr<GL::MShaderEffect> shaderProgram;

};


class MNWPSurfaceTopographyActorFactory : public MAbstractActorFactory
{
public:
    MNWPSurfaceTopographyActorFactory() : MAbstractActorFactory() {}

protected:
    MActor* createInstance() override
    { return new MNWPSurfaceTopographyActor(); }
};


} // namespace Met3D

#endif // LONLATSURFACEACTOR_H
