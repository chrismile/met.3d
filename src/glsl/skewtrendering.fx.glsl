/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2019 Marc Rautenhaus [*]
**
**  * Regional Computing Center, Visualization
**  Universitaet Hamburg, Hamburg, Germany
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

/*****************************************************************************
 ***                             UNIFORMS
 *****************************************************************************/

uniform mat4   mvpMatrix;           // model-view-projection matrix
uniform mat4   tlogp2xyMatrix;      // (t, ln(p)) coordinates to diagram (x,y)
uniform mat4   xy2worldMatrix;      // diagram (x,y) to 3D world space
uniform bool   verticesInTPSpace;   // are the vertices in (t,p) or in (x,y)?
uniform vec4   colour;              // line colour
uniform float  depthOffset;         // offset for fullscreen depth layers
uniform bool   fullscreen;          // offscreen rendering?


/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

// For fullscreen rendering: transform coordinates in diagram space (0..1) to
// view port clip space (-1..1 with padding).
vec2 transformDiagramXYToClipSpace(vec2 diagramXY)
{
//TODO (mr, 10Jan2019) -- replace by matrix multiplication.
    float hPad = 0.1;
    float vPad = 0.1;
    return vec2(diagramXY.x * (2.-2.*hPad) - 1.+hPad,
                diagramXY.y * (2.-2.*vPad) - 1.+vPad);
}


shader VSDiagramContent(in vec2 vertex : 0, out vec2 diagramXY)
{
    // 1. Transform vertex into diagram (x,y) space (0..1 x 0..1).
    // ===========================================================
    if (verticesInTPSpace)
    {
        // Map (T, p) coordinates into the diagram's (x, y) space (0..1 each).
        vec4 tlogp = vec4(vertex.x, log(vertex.y), 0, 1);
        vec4 vertexDiagramXY = tlogp2xyMatrix * tlogp;

        // Pass diagram coordinates on to fragment shader to discard fragments
        // outside of the drawing region.
        diagramXY = vec2(vertexDiagramXY.x, vertexDiagramXY.y);
    }
    else
    {
        // Vertex is already in (x,y) coordinates.
        diagramXY = vertex;
    }

    // 2. Transform diagram space into either fullscreen clip space or 3D
    // world space.
    // ==================================================================
    if (fullscreen)
    {
        // Map the diagram's (x,y) coordinates to full screen clip space.
        vec2 vertexClipSpace = transformDiagramXYToClipSpace(diagramXY);
        gl_Position = vec4(vertexClipSpace.x, vertexClipSpace.y, depthOffset, 1.);
    }
    else
    {
        // Map the diagram's (x,y) coordinates to world space...
        vec4 vertexWorldSpace = vec4(diagramXY.x, depthOffset, diagramXY.y, 1.);
        vertexWorldSpace = xy2worldMatrix * vertexWorldSpace;
        // ...and map world space to view space.
        gl_Position = mvpMatrix * vertexWorldSpace;
    }
}


/*****************************************************************************
 ***                           FRAGMENT SHADER
 *****************************************************************************/

shader FSColour(in vec2 diagramXY, out vec4 fragColour)
{
    // Discard fragments outside of the diagram drawing area.
    if (diagramXY.x < 0. || diagramXY.x > 1.
            || diagramXY.y < 0. || diagramXY.y > 1.) discard;

    fragColour = colour;
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program DiagramGeometry
{
    vs(400)=VSDiagramContent();
    fs(400)=FSColour();
};
