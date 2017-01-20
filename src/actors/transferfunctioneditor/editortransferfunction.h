/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Marc Rautenhaus
**  Copyright 2017 Fabian Schöttl
**  Copyright 2017 Bianca Tost
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
#ifndef EDITORTRANSFERFUNCTION_H
#define EDITORTRANSFERFUNCTION_H

// standard library imports

// related third party imports
#include <QPair>
#include <QVector>
#include <QColor>

// local application imports
#include "colour.h"

namespace Met3D
{
namespace TFEditor
{

enum InterpolationType
{
    HCL,
    RGB
};

/**
  @brief Interface for any type of nodes.
 */
class MAbstractNodes
{
public:
    virtual int     getNumNodes() const = 0;

    virtual float   xAt(int i) const = 0;
    virtual float   yAt(int i) const = 0;

    virtual void    setXAt(int i, float x) = 0;
    virtual void    setYAt(int i, float y) = 0;

    virtual int     addNode(float t) = 0;
    virtual void    removeNode(int i) = 0;
    virtual void    clear() = 0;
};


typedef QPair<float, MColorXYZ64> ColourNode;

/**
 * @brief Class for storing Met3D::TFEditor::ColourNode's.
 */
class MColourNodes : public MAbstractNodes
{
public:
    typedef QVector<ColourNode>::iterator iterator;
    typedef QVector<ColourNode>::const_iterator const_iterator;

    int     getNumNodes() const override;

    float   xAt(int i) const override;
    float   yAt(int i) const override;

    void    setXAt(int i, float x) override;
    void    setYAt(int i, float y) override;

    int     addNode(float t) override;
    void    removeNode(int i) override;
    void    clear() override;


    const MColorXYZ64&   colourAt(int i) const;
    MColorXYZ64&         colourAt(int i);

    void push_back(float t, const MColorXYZ64& color);

    MColorXYZ64 interpolate(float t);

    QVector<ColourNode> nodes;
    InterpolationType type;
};

typedef QPair<float, float> AlphaNode;

/**
 * @brief Class for storing Met3D::TFEditor::AlphaNode's.
 */
class MAlphaNodes : public MAbstractNodes
{
public:
    typedef QVector<AlphaNode>::iterator iterator;
    typedef QVector<AlphaNode>::const_iterator const_iterator;

    int     getNumNodes() const override;

    float   xAt(int i) const override;
    float   yAt(int i) const override;

    void    setXAt(int i, float x) override;
    void    setYAt(int i, float y) override;

    int     addNode(float t) override;
    void    removeNode(int i) override;
    void    clear() override;

    void push_back(float t, float alpha);

    float interpolate(float t);

    QVector<AlphaNode> nodes;
};


/**
 * @brief The TransferFunction class consists of color and alpha nodes as well
 * as a presampled color buffer.
 */
class MEditorTransferFunction
{
public:
    MEditorTransferFunction();

    /**
     * Samples the current color and alpha nodes and saves the result in the
     * sampled buffer.
     */
    void update(int numSamples);

    void setType(InterpolationType type);
    InterpolationType getType() const;

    const MColourNodes* getColourNodes() const;
    MColourNodes* getColourNodes();

    const MAlphaNodes* getAlphaNodes() const;
    MAlphaNodes* getAlphaNodes();

    const QVector<QRgb>* getSampledBuffer() const;

private:
    MColourNodes colourNodes;
    MAlphaNodes alphaNodes;

    QVector<QRgb> sampledBuffer;
};


} // namespace TFEditor
} // namespace Met3D

#endif // EDITORTRANSFERFUNCTION_H
