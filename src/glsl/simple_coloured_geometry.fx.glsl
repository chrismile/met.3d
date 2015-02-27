/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
**  Copyright 2015 Michael Kern
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

/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSSimple(in vec3 vertexCoord : 0)
{
    gl_Position = vec4(vertexCoord, 1);
}


/*****************************************************************************/

uniform vec3 anchor;
uniform vec2 scale;

shader VSSimpleAnchor(in vec4 vertexCoord : 0)
{
    gl_Position = vec4(anchor.x + vertexCoord.x * scale.x,
                       anchor.y + vertexCoord.y * scale.y,
                       anchor.z,
                       1);
}


/*****************************************************************************/

uniform mat4 mvpMatrix;

shader VSWorld(in vec3 vertex : 0)
{
    gl_Position = mvpMatrix * vec4(vertex, 1);
}


/*****************************************************************************/

uniform vec2 pToWorldZParams;   // parameters to convert p[hPa] to world z

shader VSPressure(in vec3 vertex : 0)
{
    // Convert pressure to world Z coordinate.
    float worldZ = (log(vertex.z) - pToWorldZParams.x) * pToWorldZParams.y;
    // Convert from world space to clip space.
    gl_Position = mvpMatrix * vec4(vertex.xy, worldZ, 1.);
}


/*****************************************************************************/

uniform float worldZ;

shader VSIsoPressure(in vec3 vertex : 0)
{
    // Convert from world space to clip space.
    gl_Position = mvpMatrix * vec4(vertex.xy, worldZ, 1.);
}


/*****************************************************************************/

// vs_vertex = lon/lat/worldZ/scalar data values
shader VSTick(in vec3 vertex : 0, out vec4 vs_vertex)
{
    // Convert pressure to world Z coordinate.
    float worldZ = (log(vertex.z) - pToWorldZParams.x) * pToWorldZParams.y;
    // Convert from world space to clip space.
    vs_vertex = vec4(vertex.xy, worldZ, 1.);
}


/*****************************************************************************
 ***                          GEOMETRY SHADER
 *****************************************************************************/

uniform vec3  offsetDirection;       // offset the second vertex by this vector

shader GSTick(in vec4 vs_vertex[])
{
    gl_Position = mvpMatrix * vs_vertex[0];
    EmitVertex();

    gl_Position = mvpMatrix * vec4(vs_vertex[0].xyz + offsetDirection, 1.);
    EmitVertex();
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

uniform vec4 colour;

shader FSmain(out vec4 fragColour)
{
    fragColour = colour;
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Simple
{
    vs(400)=VSSimple();
    fs(400)=FSmain();
};


program SimpleAnchor
{
    vs(400)=VSSimpleAnchor();
    fs(400)=FSmain();
};


// Geometry given in WorldSpace
program WorldSpace
{
    vs(420)=VSWorld();
    fs(420)=FSmain();
};


// Geometry given in Pressure
program Pressure
{
    vs(420)=VSPressure();
    fs(420)=FSmain();
};

// Geometry with given WorldZ coord for iso-pressure
program IsoPressure
{
    vs(420)=VSIsoPressure();
    fs(420)=FSmain();
};

// Program to extrude ticks at the edges of the bounding box
program TickMarks
{
    vs(400)=VSTick();
    gs(400)=GSTick() : in(points), out(line_strip, max_vertices = 2);
    fs(400)=FSmain();
};
