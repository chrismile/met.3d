/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2018 Marc Rautenhaus
**  Copyright 2015      Michael Kern
**  Copyright 2017-2018 Bianca Tost
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
******************************************************************************/

#include "hsec_sample_data_routine.fx.glsl"

/*****************************************************************************
 ***                             CONSTANTS
 *****************************************************************************/

const float MS_TO_KNOTS = 1.9438444924574;


/*****************************************************************************
 ***                             INTERFACES
 *****************************************************************************/

interface GStoFS
{
    smooth vec4 color;
};


/*****************************************************************************
 ***                             STRUCTS
 *****************************************************************************/

struct VectorFieldData
{
    vec3 dir;               // normalised vector field direction
    vec3 normal;            // normal vector of vector field direction
    float magnitude;        // magnitude of vector field
    float deltaGrid;        // scaling factor
    float adaptedLineWidth; // line width adapted to scaling factor (delta grid)
    float glyphLength;      // adapted glyph length
} vectorField;


/*****************************************************************************
 ***                             UNIFORMS
 *****************************************************************************/

uniform mat4        mvpMatrix;
uniform float       deltaGridX; // delta between two points in x
uniform float       deltaGridY; // delta between two points in y
uniform float       worldZ;     // world z coordinate of this section

// Sample textures for longitudial/latitudial component of vector field.
uniform sampler3D   vectorDataLonComponent3D;
uniform sampler3D   vectorDataLatComponent3D;

uniform sampler2D   vectorDataLonComponent2D;
uniform sampler2D   vectorDataLatComponent2D;

uniform sampler2D   surfacePressureLon;    // surface pressure field in Pa
uniform sampler1D   hybridCoefficientsLon; // hybrid sigma pressure coefficients
uniform sampler2D   surfacePressureLat;    // surface pressure field in Pa
uniform sampler1D   hybridCoefficientsLat; // hybrid sigma pressure coefficients
uniform sampler3D   auxPressureFieldLon_hPa; // 3D pressure field in hPa
uniform sampler3D   auxPressureFieldLat_hPa; // 3D pressure field in hPa

// Adjustable glyph parameters.
uniform float       lineWidth;
uniform vec4        glyphColor;
uniform int         pivotPosition;

// Adjustable wind barbs parameters.
uniform bool        showCalmGlyph;
uniform int         numFlags;

// Adjustable arrows parameters.
uniform float       arrowHeadHeight;
uniform bool        arrowDrawOutline;
uniform float       arrowOutlineWidth;
uniform vec4        arrowOutlineColour;
uniform bool        scaleArrowWithMagnitude;
uniform float       scaleArrowMinScalar;
uniform float       scaleArrowMinLength;
uniform bool        scaleArrowDiscardBelow;
uniform float       scaleArrowMaxScalar;
uniform float       scaleArrowMaxLength;
uniform bool        scaleArrowDiscardAbove;


/*****************************************************************************
 ***                             INCLUDES
 *****************************************************************************/

float computeKnots(in float velMeterPerSecond)
{
    return velMeterPerSecond * MS_TO_KNOTS;
}


// Compute vector field specific data (vector field direction, magnitude,
// direction normal, etc.) at posWorld used to render both wind barbs and arrow
// glyphs.
bool computeVectorFieldData(in vec3 posWorld)
{
    // Sample both vector field component textures, obtain scalar value in
    // longitudial and latitudial direction.
    float lonField = sampleDataAtPos(
                posWorld, vectorDataLonComponent3D, vectorDataLonComponent2D,
                surfacePressureLon, hybridCoefficientsLon,
                auxPressureFieldLon_hPa);
    float latField = sampleDataAtPos(
                posWorld, vectorDataLatComponent3D, vectorDataLatComponent2D,
                surfacePressureLat, hybridCoefficientsLat,
                auxPressureFieldLat_hPa);

    // If no data is available, do not draw any glyphs.
    if (lonField == MISSING_VALUE || latField == MISSING_VALUE)
    {
        return false;
    }

    // Optain vector field direction at posWorld (e.g. wind).
    vec3 magnitudeDir = vec3(lonField, latField, 0);

    // Normalize direction vector.
    vectorField.dir = normalize(magnitudeDir);
    vectorField.normal = normalize(vec3(-vectorField.dir.y, vectorField.dir.x,
                                        0));

    // Get vector magnitude (e.g. wind speed).
    vectorField.magnitude = length(magnitudeDir);

    // distance from camera to glyph position
    //float camDist = cameraPosition.z - pos.z;
    // clamp distance factor from 0 to 2
    //camDist = clamp(camDist / 50.0 + 1, 0.25, 10.0);
    // compute scale factor [1; 3.5]
    //const float scaleDistFactor = camDist * 0.6;

    // Compute useful glyph parameters.
    vectorField.deltaGrid = (deltaGridX + deltaGridY) / 2.0f;

    // Adapt line width of glyph elements to (automatic) scaling.
    vectorField.adaptedLineWidth = vectorField.deltaGrid * lineWidth;
    // Compute glyph length.
    vectorField.glyphLength = 0.75 * vectorField.deltaGrid;

    return true;
}


// Determine the positions of the four vertices of the glyph's base line
// located at pos. The base line is a line in vector field direction at which
// the arrow head / wind barb pennants are attached. The positions are written
// to pos0, pos1, pos2, pos3.
void determineBaseLineVertices(in vec3 pos, out vec3 pos0, out vec3 pos1,
                               out vec3 pos2, out vec3 pos3)
{
    // Place pivot at glyph tip.
    if (pivotPosition == 0)
    {
        // Calculate position of left bottom vertex.
        pos0 = pos - vectorField.dir * vectorField.glyphLength
                - vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of left top vertex.
        pos1 = pos - vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of right bottom vertex.
        pos2 = pos - vectorField.dir * vectorField.glyphLength
                + vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of right top vertex.
        pos3 = pos + vectorField.normal * vectorField.adaptedLineWidth / 2.0;
    }
    // Place pivot at glyph centre.
    else if (pivotPosition == 1)
    {
        // Calculate position of left bottom vertex.
        pos0 = pos - vectorField.dir * vectorField.glyphLength / 2.0
                - vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of left top vertex.
        pos1 = pos + vectorField.dir * vectorField.glyphLength / 2.0
                - vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of right bottom vertex.
        pos2 = pos - vectorField.dir * vectorField.glyphLength / 2.0
                + vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of right top vertex.
        pos3 = pos + vectorField.dir * vectorField.glyphLength / 2.0
                + vectorField.normal * vectorField.adaptedLineWidth / 2.0;
    }
    // Place pivot at glyph tail.
    else if (pivotPosition == 2)
    {
        // Calculate position of left bottom vertex.
        pos0 = pos - vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of left top vertex.
        pos1 = pos + vectorField.dir * vectorField.glyphLength
                - vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of right bottom vertex.
        pos2 = pos + vectorField.normal * vectorField.adaptedLineWidth / 2.0;
        // Calculate position of right top vertex.
        pos3 = pos + vectorField.dir * vectorField.glyphLength
                + vectorField.normal * vectorField.adaptedLineWidth / 2.0;
    }
}


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain(in vec2 worldXY, out vec3 worldPos)
{
    worldPos.x = worldXY.x;
    worldPos.y = worldXY.y;
    worldPos.z = worldZ;
}


/*****************************************************************************
 ***                          GEOMETRY SHADER
 *****************************************************************************/


shader GSwindBarbs(in vec3 worldPos[], out GStoFS Output)
{
    Output.color = glyphColor;

    // Current world position.
    vec3 posWorld = worldPos[0];

    if (!computeVectorFieldData(posWorld))
    {
        return;
    }

    // Convert wind velocity in m/s into kt.
    float knots = computeKnots(vectorField.magnitude);

    // Compute small radius of calm wind.
    float smallRadius = 0.15 * vectorField.deltaGrid;
    // Compute large radius of calm wind.
    float largeRadius = 0.3 * vectorField.deltaGrid;

    // Set posOffset.
    float posOffset = vectorField.glyphLength / numFlags;

    // Set pennant triangle offset.
    float pennantPeakOffset = 0.3 * posOffset;
    // Set offset of flags and pennant.
    float pennantOffset = 2 * 0.95 * posOffset;
    // Set flag offset.
    float flagOffset = pennantOffset;

    // Compute width of small flag.
    float smallWidth = 0.2 * 0.6 * vectorField.deltaGrid;
    // Compute width of large flag.
    float largeWidth = 0.2 * vectorField.deltaGrid;

    // Compute distance between elements.
    //const float pennantDist = (offset  + pennantOffset);
    //const float flagDist = offset * 1    * scaleDistFactor;

    //! NOTE (Kern, 2016-11-03): Using EmitVertex() and EndPrimitve() in local
    // functions is not possible after NVidia driver 340 since the latest driver
    // do not allow to use stage-specific functions in all shaders. GLFX always
    // includes local functions into all shader stage programs (VS/FS) which
    // leads to compilation errors for all stages except the geometry shader.

// -----------------------------------------------------------------------------
// Create the base line of the wind barb glyph.

    if (knots >= 5.0)
    {
        vec3 pos0, pos1, pos2, pos3;
        vec3 pos = posWorld;

        determineBaseLineVertices(pos, pos0, pos1, pos2, pos3);

        gl_Position = mvpMatrix * vec4(pos0, 1);
        EmitVertex();
        gl_Position = mvpMatrix * vec4(pos1, 1);
        EmitVertex();
        gl_Position = mvpMatrix * vec4(pos2, 1);
        EmitVertex();
        gl_Position = mvpMatrix * vec4(pos3, 1);
        EmitVertex();
        EndPrimitive();

        vec3 elemPos;
        // Place pivot at glyph tip.
        if (pivotPosition == 0)
        {
            elemPos = posWorld - vectorField.dir * vectorField.glyphLength
                    + vectorField.dir * (posOffset / 2.0);
        }
        // Place pivot at glyph centre.
        else if (pivotPosition == 1)
        {
            elemPos = posWorld
                    - vectorField.dir * (vectorField.glyphLength / 2.0 * 0.9)
                    + vectorField.dir * (posOffset / 2.0);
        }
        // Place pivot at glyph tail.
        else if (pivotPosition == 2)
        {
            elemPos = posWorld + vectorField.dir * (posOffset / 2.0);
        }

// -----------------------------------------------------------------------------
// Create the pennants at the current wind barb position.

        for(int i = 0; i < floor(knots / 50); ++i)
        {
            vec3 pos0, pos1, pos2;
            vec3 pos = elemPos;

            //float scaleFactor = vectorField.adaptedLineWidth / lineWidth;
            pos += vectorField.normal * vectorField.adaptedLineWidth / 2.0;

            pos0 = pos - posOffset / 2.0 * vectorField.dir;
            pos1 = pos0 - vectorField.dir * posOffset / 2.0
                    + vectorField.dir * pennantPeakOffset
                    + vectorField.normal * largeWidth;
            pos2 = pos0 - vectorField.dir * posOffset / 2.0
                    + vectorField.dir * (pennantOffset + pennantPeakOffset);

            gl_Position = mvpMatrix * vec4(pos0, 1);
            EmitVertex();
            gl_Position = mvpMatrix * vec4(pos1, 1);
            EmitVertex();
            gl_Position = mvpMatrix * vec4(pos2, 1);
            EmitVertex();
            EndPrimitive();

            elemPos += 2 * posOffset * vectorField.dir;
        }

        knots = mod(knots,50);

        //elemPos += flagDist * 0.001 * vectorField.dir;

// -----------------------------------------------------------------------------
// Create the full flag triangle at the current wind barb position.

        for(int j = 0; j < floor(knots / 10); ++j)
        {
            vec3 pos0, pos1, pos2, pos3;
            vec3 pos = elemPos;

            pos += vectorField.normal * vectorField.adaptedLineWidth / 2.0;

            // left bottom
            pos0 = pos - vectorField.dir * vectorField.adaptedLineWidth / 2.0;
            // left top
            pos1 = pos0 - vectorField.dir * flagOffset
                    + vectorField.normal * largeWidth;
            // right bottom
            pos2 = pos + vectorField.dir * vectorField.adaptedLineWidth / 2.0;
            // right top
            pos3 = pos2 - vectorField.dir * flagOffset
                    + vectorField.normal * largeWidth;

            gl_Position = mvpMatrix * vec4(pos0, 1);
            EmitVertex();
            gl_Position = mvpMatrix * vec4(pos1, 1);
            EmitVertex();
            gl_Position = mvpMatrix * vec4(pos2, 1);
            EmitVertex();
            gl_Position = mvpMatrix * vec4(pos3, 1);
            EmitVertex();
            EndPrimitive();


            elemPos += posOffset * vectorField.dir;
        }

        knots = mod(knots,10);

// -----------------------------------------------------------------------------
// Create the half flag triangle at the current wind barb position.

        for(int k = 0; k < floor(knots / 5); ++k)
        {
            vec3 pos0, pos1, pos2, pos3;
            vec3 pos = elemPos;

            pos += vectorField.normal * vectorField.adaptedLineWidth / 2.0;

            // left bottom
            pos0 = pos - vectorField.dir * vectorField.adaptedLineWidth / 2.0;
            // left top
            pos1 = pos0 - vectorField.dir * flagOffset * 0.6
                    + vectorField.normal * smallWidth;
            // right bottom
            pos2 = pos + vectorField.dir * vectorField.adaptedLineWidth / 2.0;
            // right top
            pos3 = pos2 - vectorField.dir * flagOffset * 0.6
                    + vectorField.normal * smallWidth;

            gl_Position = mvpMatrix * vec4(pos0, 1);
            EmitVertex();
            gl_Position = mvpMatrix * vec4(pos1, 1);
            EmitVertex();
            gl_Position = mvpMatrix * vec4(pos2, 1);
            EmitVertex();
            gl_Position = mvpMatrix * vec4(pos3, 1);
            EmitVertex();
            EndPrimitive();

            elemPos += posOffset * vectorField.dir;
        }

        knots = mod(knots,5);
   }
   else
   {
// -----------------------------------------------------------------------------
// Create the glyph for calm wind with 2 circles consiting of 128 vertices each.
        
        if (showCalmGlyph)
        {

            vec3 pos = posWorld;

            const int tubeSegments = 16;
            const float angleStep = 360.0 / float(tubeSegments);
            const vec3 normal = vec3(1,0,0);
            const vec3 binormal = vec3(0,1,0);

            vec3 prevPos0, prevPos1;
            vec3 currPos0, currPos1;
            for (int i = 0; i <= tubeSegments; ++i)
            {
                float angle = radians(angleStep * i);
                float cosi = cos(angle) * smallRadius;
                float sini = sin(angle) * smallRadius;
                float sinig = sin(angle)
                        * (smallRadius - vectorField.adaptedLineWidth);
                float cosig = cos(angle)
                        * (smallRadius - vectorField.adaptedLineWidth);

                if (i == 0)
                {
                    prevPos0 = pos + normal * cosi + binormal * sini;
                    prevPos1 = pos + normal * cosig + binormal * sinig;
                    continue;
                }

                currPos0 = pos + normal * cosi + binormal * sini;
                currPos1 = pos + normal * cosig + binormal * sinig;

                gl_Position = mvpMatrix * vec4(prevPos0, 1);
                EmitVertex();
                gl_Position = mvpMatrix * vec4(currPos0, 1);
                EmitVertex();
                gl_Position = mvpMatrix * vec4(prevPos1, 1);
                EmitVertex();
                gl_Position = mvpMatrix * vec4(currPos1, 1);
                EmitVertex();
                EndPrimitive();

                prevPos0 = currPos0;
                prevPos1 = currPos1;
            }

            for (int i = 0; i <= tubeSegments; ++i)
            {
                float angle = radians(angleStep * i);
                float cosi = cos(angle) * largeRadius;
                float sini = sin(angle) * largeRadius;
                float sinig = sin(angle)
                        * (largeRadius - vectorField.adaptedLineWidth);
                float cosig = cos(angle)
                        * (largeRadius - vectorField.adaptedLineWidth);

                if (i == 0)
                {
                    prevPos0 = pos + normal * cosi + binormal * sini;
                    prevPos1 = pos + normal * cosig + binormal * sinig;
                    continue;
                }

                currPos0 = pos + normal * cosi + binormal * sini;
                currPos1 = pos + normal * cosig + binormal * sinig;

                gl_Position = mvpMatrix * vec4(prevPos0, 1);
                EmitVertex();
                gl_Position = mvpMatrix * vec4(currPos0, 1);
                EmitVertex();
                gl_Position = mvpMatrix * vec4(prevPos1, 1);
                EmitVertex();
                gl_Position = mvpMatrix * vec4(currPos1, 1);
                EmitVertex();
                EndPrimitive();

                prevPos0 = currPos0;
                prevPos1 = currPos1;
            }
        }

// -----------------------------------------------------------------------------
   }
}


shader GSarrows(in vec3 worldPos[], out GStoFS Output)
{
    Output.color = glyphColor;

    // Current world position.
    vec3 posWorld = worldPos[0];

    if (!computeVectorFieldData(posWorld))
    {
        return;
    }

    // distance between elements
    //const float pennantDist = (offset  + pennantOffset);
    //const float flagDist = offset * 1    * scaleDistFactor;

    //! NOTE (Kern, 2016-11-03): Using EmitVertex() and EndPrimitve() in local
    // functions is not possible after NVidia driver 340 since the latest driver
    // do not allow to use stage-specific functions in all shaders. GLFX always
    // includes local functions into all shader stage programs (VS/FS) which
    // leads to compilation errors for all stages except the geometry shader.

    // -------------------------------------------------------------------------
    // Create the base line of the arrow glyph.

    vec3 pos0, pos1, pos2, pos3;
    vec3 pos = posWorld;

    if (scaleArrowWithMagnitude)
    {
        if (scaleArrowDiscardBelow && vectorField.magnitude < scaleArrowMinScalar)
        {
            return;
        }
        if (scaleArrowDiscardAbove && vectorField.magnitude > scaleArrowMaxScalar)
        {
            return;
        }

        float scalar = min(max(vectorField.magnitude, scaleArrowMinScalar),
                           scaleArrowMaxScalar);
        float scalarScale = (scalar - scaleArrowMinScalar)
                / (scaleArrowMaxScalar - scaleArrowMinScalar);
        float lengthScale =
                scaleArrowMinLength + scalarScale *
                (scaleArrowMaxLength - scaleArrowMinLength);

        vectorField.glyphLength = lengthScale * vectorField.deltaGrid;
    }

    determineBaseLineVertices(pos, pos0, pos1, pos2, pos3);

    float arrowHeight = min(vectorField.glyphLength,
                            arrowHeadHeight * vectorField.deltaGrid);

    if (arrowHeight == 0.)
    {
        return;
    }

    vec3 posTip = pos1 + vectorField.normal * vectorField.adaptedLineWidth / 2.f;
    vec3 posBase = posTip - vectorField.dir * arrowHeight;
    vec3 sideVec = vectorField.normal * (arrowHeight);
    vec3 posLeft = posBase - sideVec;
    vec3 posRight = posBase + sideVec;

    pos1 = pos1 - vectorField.dir * arrowHeight;
    pos3 = pos3 - vectorField.dir * arrowHeight;


    // Draw base line.

    gl_Position = mvpMatrix * vec4(pos0, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos1, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos2, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos3, 1);
    EmitVertex();
    EndPrimitive();

    // Draw arrowhead.

    gl_Position = mvpMatrix * vec4(posBase + sideVec, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(posTip, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(posBase, 1);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(posBase - sideVec, 1);
    EmitVertex();
    EndPrimitive();

    if (!arrowDrawOutline)
    {
        return;
    }

    // Draw outline as triangle strip. Don't draw at the same position as the
    // inside of the arrow since this leads to z-fighting.

    float adaptedArrowOutlineWidth = vectorField.deltaGrid * arrowOutlineWidth;

    vec3 outlineOffsetDir = adaptedArrowOutlineWidth * vectorField.dir;
    vec3 outlineOffsetNormal = adaptedArrowOutlineWidth * vectorField.normal;

    vec3 pos0Out = pos0 - outlineOffsetDir - outlineOffsetNormal;
    vec3 pos1Out = pos1 - outlineOffsetDir - outlineOffsetNormal;
    vec3 pos2Out = pos2 - outlineOffsetDir + outlineOffsetNormal;
    vec3 pos3Out = pos3 - outlineOffsetDir + outlineOffsetNormal;

    // Since the arrowhead is a rectangular and isosceles triangle, the
    // positions of the outline vertices can be computed with the help of
    // parallelograms and rectangular and isosceles triangles.
    vec3 posTipOut = posTip + (sqrt(2) * outlineOffsetDir);
    vec3 posBaseOut = posBase - outlineOffsetDir;
    vec3 sideVecOut = sideVec + (sqrt(2.) + 1.) * adaptedArrowOutlineWidth
            * vectorField.normal;
    vec3 posLeftOut = posBaseOut - sideVecOut;
    vec3 posRightOut = posBaseOut + sideVecOut;

    Output.color = arrowOutlineColour;

    // Triangle strip which defines the outline.

    gl_Position = mvpMatrix * vec4(posLeftOut, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(posLeft, 1.);
    EmitVertex();

    gl_Position = mvpMatrix * vec4(posTipOut, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(posTip, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(posRightOut, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(posRight, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos3Out, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos3, 1.);
    EmitVertex();
    // Needs to be doubled since otherwise the outline won't be rendered
    // correctly.
    gl_Position = mvpMatrix * vec4(pos3Out, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos2, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos2Out, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos0, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos0Out, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos1, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(pos1Out, 1.);
    EmitVertex();

    gl_Position = mvpMatrix * vec4(posLeft, 1.);
    EmitVertex();
    gl_Position = mvpMatrix * vec4(posLeftOut, 1.);
    EmitVertex();
    EndPrimitive();
}

/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

shader FSmain(in GStoFS Input, out vec4 fragColor)
{
    fragColor = Input.color;
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program WindBarbs
{
    vs(330)=VSmain();
    gs(330)=GSwindBarbs() : in(points), out(triangle_strip, max_vertices = 128);
    fs(330)=FSmain();
};

program Arrows
{
    vs(330)=VSmain();
    gs(330)=GSarrows() : in(points), out(triangle_strip, max_vertices = 128);
    fs(330)=FSmain();
};

program ArrowsOutline
{
    vs(330)=VSmain();
    gs(330)=GSarrows() : in(points), out(line_strip, max_vertices = 128);
//    gs(330)=GSarrows() : in(points), out(triangle_strip, max_vertices = 128);
    fs(330)=FSmain();
};
