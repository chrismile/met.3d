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
#include "editortransferfunction.h"

// standard library imports

// related third party imports

// local application imports
#include "colourpicker.h"

namespace Met3D
{
namespace TFEditor
{

/******************************************************************************
***                              LOCAL METHODS                              ***
*******************************************************************************/
// Searches for the two nodes nearest to t with i1 smaller t and i2 greater t.
template<typename T>
void findIteratorPair(const T& nodes, typename T::const_iterator& i1,
                      typename T::const_iterator& i2, float t)
{
    i1 = nodes.end();
    for (auto i = nodes.begin(); i !=nodes.end(); i++)
    {
        if (i->first <= t)
        {
            if (i1 == nodes.end() || i->first > i1->first)
            {
                i1 = i;
            }
        }
    }

    i2 = nodes.end();
    for (auto i = nodes.begin(); i !=nodes.end(); i++)
    {
        if (i->first >= t)
        {
            if (i2 == nodes.end() || i->first < i2->first)
            {
                i2 = i;
            }
        }
    }
}


/******************************************************************************
***                              MColourNodes                               ***
*******************************************************************************/
/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

int MColourNodes::getNumNodes() const
{
    return nodes.size();
}


float MColourNodes::xAt(int i) const
{
    return nodes[i].first;
}


float MColourNodes::yAt(int i) const
{
    Q_UNUSED(i);
    return 0.5f;
}


void MColourNodes::setXAt(int i, float x)
{
    nodes[i].first = x;
}


void MColourNodes::setYAt(int i, float y)
{
    Q_UNUSED(i);
    Q_UNUSED(y);
}


int MColourNodes::addNode(float t)
{
    int i = nodes.size();

    MColourXYZ64 color = interpolate(t);
    nodes.push_back({t, color});

    return i;
}


void MColourNodes::removeNode(int i)
{
    nodes.remove(i);
}


void MColourNodes::clear()
{
    nodes.clear();
}


const MColourXYZ64& MColourNodes::colourAt(int i) const
{
    return nodes[i].second;
}


MColourXYZ64& MColourNodes::colourAt(int i)
{
    return nodes[i].second;
}


void MColourNodes::push_back(float t, const MColourXYZ64& color)
{
    nodes.push_back(ColourNode(t, color));
}


MColourXYZ64 MColourNodes::interpolate(float t)
{
    MColourNodes::const_iterator i1, i2;
    findIteratorPair(nodes, i1, i2, t);

    // Return 0 if one node could not be found (should not happen).
    if (i1 == nodes.end() || i2 == nodes.end())
    {
        return MColourXYZ64();
    }

    if (i1 == i2)
    {
        return i1->second;
    }

    float l = (t - i1->first) / (i2->first - i1->first);

    if (interpolationColourSpace == HCL)
    {
        return lerpHCL(i1->second, i2->second, l);
    }
    else if (interpolationColourSpace == RGB)
    {
        return lerpRGB(i1->second, i2->second, l);
    }
    else
    {
        return lerp(i1->second, i2->second, l);
    }
}


/******************************************************************************
***                               MAlphaNodes                               ***
*******************************************************************************/
/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

int MAlphaNodes::getNumNodes() const
{
    return nodes.size();
}


float MAlphaNodes::xAt(int i) const
{
    return nodes[i].first;
}


float MAlphaNodes::yAt(int i) const
{
    return nodes[i].second;
}


void MAlphaNodes::setXAt(int i, float x)
{
    nodes[i].first = x;
}


void MAlphaNodes::setYAt(int i, float y)
{
    nodes[i].second = y;
}


int MAlphaNodes::addNode(float t)
{
    int i = nodes.size();

    float alpha = interpolate(t);
    nodes.push_back({t, alpha});

    return i;
}


void MAlphaNodes::removeNode(int i)
{
    nodes.remove(i);
}


void MAlphaNodes::clear()
{
    nodes.clear();
}


void MAlphaNodes::push_back(float t, float alpha)
{
    nodes.push_back(AlphaNode(t, alpha));
}


float MAlphaNodes::interpolate(float t)
{
    MAlphaNodes::const_iterator i1, i2;
    findIteratorPair(nodes, i1, i2, t);

    // Return 0 if one node could not be found (should not happen).
    if (i1 == nodes.end() || i2 == nodes.end())
    {
        return 0;
    }

    if (i1 == i2)
    {
        return i1->second;
    }

    float l = (t - i1->first) / (i2->first - i1->first);

    return i1->second * (1 - l) + i2->second * l;
}


/******************************************************************************
***                         MEditorTransferFunction                         ***
*******************************************************************************/
/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MEditorTransferFunction::MEditorTransferFunction()
{
    // Add first and last nodes as first two nodes in nodes-vector accordingly.

	colourNodes.nodes = QVector<ColourNode>()
		<< ColourNode(0.f, (MColourXYZ64)(MColourHCL16(6.f, 80.f, 28.f)))
		<< ColourNode(1.f, (MColourXYZ64)(MColourHCL16(90.f, 5.f, 86.f)));
	alphaNodes.nodes = QVector<AlphaNode>()
		<< AlphaNode(0.f, 1.f)
		<< AlphaNode(1.f, 1.f);
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MEditorTransferFunction::update(int numSamples)
{
    sampledBuffer.resize(numSamples);

    float min = colourNodes.xAt(0);
    float max = colourNodes.xAt(1);

    // Iterate over all samples and calculate colour. (like colour bar)
    for (int i = 0; i < numSamples; i++)
    {
        float l = i / float(numSamples - 1);
        l = l * (max - min) + min;

        MColourXYZ64 color = colourNodes.interpolate(l);
        float alpha = alphaNodes.interpolate(l);

        MColourRGB8 rgb = (MColourRGB8)color;

        sampledBuffer[i] = qRgba(rgb.r, rgb.g, rgb.b, int(alpha * 255));
    }
}


void MEditorTransferFunction::setCSpaceForCNodeInterpolation(ColourSpaceForColourNodeInterpolation type)
{
    colourNodes.interpolationColourSpace = type;
}


ColourSpaceForColourNodeInterpolation MEditorTransferFunction::getCSpaceForCNodeInterpolation() const
{
    return colourNodes.interpolationColourSpace;
}


const MColourNodes* MEditorTransferFunction::getColourNodes() const
{
    return &colourNodes;
}


MColourNodes* MEditorTransferFunction::getColourNodes()
{
    return &colourNodes;
}


const MAlphaNodes* MEditorTransferFunction::getAlphaNodes() const
{
    return &alphaNodes;
}


MAlphaNodes* MEditorTransferFunction::getAlphaNodes()
{
    return &alphaNodes;
}


const QVector<QRgb>* MEditorTransferFunction::getSampledBuffer() const
{
    return &sampledBuffer;
}


} // namespace TFEditor
} // namespace Met3D
