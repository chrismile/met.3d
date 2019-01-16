/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2015-2017 Michael Kern
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
 ***                             INTERFACES
 *****************************************************************************/

interface VStoGS
{
    smooth vec3 worldSpaceCoordinate;
    smooth float value;
};

interface GStoFSLine
{
    smooth float valuePS;
};

interface GStoFS
{
    smooth vec3 worldPos;
    smooth vec3 normalPS;
    smooth float valuePS;
};

/*****************************************************************************
 ***                             UNIFORMS
 *****************************************************************************/

uniform float   tubeRadius;
uniform vec3    tubeColor;
uniform vec2    pToWorldZParams;
uniform int     colorMode;

/*****************************************************************************
 ***                           VERTEX SHADER
 *****************************************************************************/

shader VSmain(in vec3 vertex0 : 0, in float value0 : 1, out VStoGS Output)
{
    Output.worldSpaceCoordinate = vertex0;
    Output.value = value0;
}

shader VSTrajectoryMain(in vec3 vertex0 : 0, in float value0 : 1,
                        out VStoGS Output)
{
    float worldZ = (log(vertex0.z) - pToWorldZParams.x) * pToWorldZParams.y;

    Output.worldSpaceCoordinate = vec3(vertex0.xy, worldZ);

    Output.value = (colorMode == 1) ? vertex0.z : value0;
}


shader VSShadow(in vec3 vertex0 : 0, in float value0 : 1, out VStoGS Output)
{
    Output.worldSpaceCoordinate = vec3(vertex0.xy, 0.05);
    Output.value = value0;
}


/*****************************************************************************
 ***                          GEOMETRY SHADER
 *****************************************************************************/

uniform mat4    mvpMatrix;

shader GSLine(in VStoGS Input[], out GStoFSLine Output)
{
    vec3 pos = Input[0].worldSpaceCoordinate;

    if(Input[1].value == -1 || Input[2].value == -1)
        return;

    gl_Position = mvpMatrix * vec4(Input[1].worldSpaceCoordinate, 1);
    Output.valuePS = Input[1].value;
    EmitVertex();

    gl_Position = mvpMatrix * vec4(Input[2].worldSpaceCoordinate, 1);
    Output.valuePS = Input[2].value;
    EmitVertex();

    EndPrimitive();
}


/*****************************************************************************/

// calculate new basis
void calculateRayBasis( in vec3 prevPos,
                        in vec3 pos,
                        out vec3 normal,
                        out vec3 binormal)
{
    vec3 lineDir = normalize(pos - prevPos);

    normal = cross(lineDir, vec3(0, 0, 1));
    if (length(normal) <= 0.01)
    {
        normal = cross(lineDir, vec3(1, 0, 0));

        if (length(normal) <= 0.01)
        {
            normal = cross(lineDir, vec3(0, 0, 1));
        }
    }

    normal = normalize(normal);

    binormal = cross(lineDir,normal);
    binormal = normalize(binormal);

    normal = cross(binormal, lineDir);
    normal = normalize(normal);
}


shader GSBox(in VStoGS Input[], out GStoFS Output)
{
    if (Input[1].value == -1 || Input[2].value == -1)
        return;

    vec3 start_position = vec3(-1,-1,-1);
    if (Input[0].value == -1)
        start_position = Input[1].worldSpaceCoordinate;

    vec3 gradient = Input[1].worldSpaceCoordinate - Input[2].worldSpaceCoordinate;

    vec3 prev_ray_position = Input[1].worldSpaceCoordinate;
    vec3 ray_position = Input[2].worldSpaceCoordinate;
    float prev_value = Input[1].value;
    float value = Input[2].value;

    bool inverse = false;

    vec3 normal, binormal;
    calculateRayBasis(prev_ray_position, ray_position, normal, binormal);

    Output.valuePS = prev_value;

    float magnifier;

    if (prev_ray_position == start_position)
        magnifier = 3.5;
    else
        magnifier = 3.0;

    vec3 prev_ray_pos_ll = prev_ray_position + tubeRadius * magnifier * (-normal - binormal);
    vec3 prev_ray_pos_ul = prev_ray_position + tubeRadius * magnifier * (-normal + binormal);
    vec3 prev_ray_pos_lr = prev_ray_position + tubeRadius * magnifier * ( normal - binormal);
    vec3 prev_ray_pos_ur = prev_ray_position + tubeRadius * magnifier * ( normal + binormal);

    Output.normalPS = gradient;

    gl_Position = mvpMatrix * vec4(prev_ray_pos_ll,1);
    Output.worldPos = prev_ray_pos_ll;
    EmitVertex();
    gl_Position = mvpMatrix * vec4(prev_ray_pos_ul,1);
    Output.worldPos = prev_ray_pos_ul;
    EmitVertex();
    gl_Position = mvpMatrix * vec4(prev_ray_pos_lr,1);
    Output.worldPos = prev_ray_pos_lr;
    EmitVertex();
    gl_Position = mvpMatrix * vec4(prev_ray_pos_ur,1);
    Output.worldPos = prev_ray_pos_ur;
    EmitVertex();
    EndPrimitive();

    prev_ray_pos_ll = prev_ray_position + tubeRadius * (-normal - binormal);
    prev_ray_pos_ul = prev_ray_position + tubeRadius * (-normal + binormal);
    prev_ray_pos_lr = prev_ray_position + tubeRadius * ( normal - binormal);
    prev_ray_pos_ur = prev_ray_position + tubeRadius * ( normal + binormal);

    vec3 ray_pos_ll = ray_position + tubeRadius * (-normal - binormal);
    vec3 ray_pos_ul = ray_position + tubeRadius * (-normal + binormal);
    vec3 ray_pos_lr = ray_position + tubeRadius * ( normal - binormal);
    vec3 ray_pos_ur = ray_position + tubeRadius * ( normal + binormal);

    Output.normalPS = -normal - binormal;
    Output.valuePS = prev_value;
    gl_Position = mvpMatrix * vec4(prev_ray_pos_ll,1);
    Output.worldPos = prev_ray_pos_ll;
    EmitVertex();
    Output.normalPS = -normal - binormal;
    Output.valuePS = value;
    gl_Position = mvpMatrix * vec4(ray_pos_ll,1);
    Output.worldPos = ray_pos_ll;
    EmitVertex();
    Output.normalPS = normal - binormal;
    Output.valuePS = prev_value;
    gl_Position = mvpMatrix * vec4(prev_ray_pos_lr,1);
    Output.worldPos = prev_ray_pos_lr;
    EmitVertex();
    Output.normalPS = normal - binormal;
    Output.valuePS = value;
    gl_Position = mvpMatrix * vec4(ray_pos_lr,1);
    Output.worldPos = ray_pos_lr;
    EmitVertex();
    Output.normalPS = normal + binormal;
    Output.valuePS = prev_value;
    gl_Position = mvpMatrix * vec4(prev_ray_pos_ur,1);
    Output.worldPos = prev_ray_pos_ur;
    EmitVertex();
    Output.normalPS = normal + binormal;
    Output.valuePS = value;
    gl_Position = mvpMatrix * vec4(ray_pos_ur,1);
    Output.worldPos = ray_pos_ur;
    EmitVertex();
    Output.normalPS = -normal + binormal;
    Output.valuePS = prev_value;
    gl_Position = mvpMatrix * vec4(prev_ray_pos_ul,1);
    Output.worldPos = prev_ray_pos_ul;
    EmitVertex();
    Output.normalPS = -normal + binormal;
    Output.valuePS = value;
    gl_Position = mvpMatrix * vec4(ray_pos_ul,1);
    Output.worldPos = ray_pos_ul;
    EmitVertex();
    Output.normalPS = -normal - binormal;
    Output.valuePS = prev_value;
    gl_Position = mvpMatrix * vec4(prev_ray_pos_ll,1);
    Output.worldPos = prev_ray_pos_ll;
    EmitVertex();
    Output.normalPS = -normal - binormal;
    Output.valuePS = value;
    gl_Position = mvpMatrix * vec4(ray_pos_ll,1);
    Output.worldPos = ray_pos_ll;
    EmitVertex();
    EndPrimitive();
}


/*****************************************************************************/

uniform bool toDepth;
uniform sampler2D depthTex;

shader GSTube(in VStoGS Input[], out GStoFS Output)
{

    Output.valuePS = 0;

    bool startPrimitive = gl_PrimitiveIDIn == 0;

    if (Input[1].value == -1 || Input[2].value == -1)
    {
        return;
    }
    //    o ----o______o-----o
    vec3 pos0, pos1, pos2, pos3;

    float value1, value2;

    value1 = Input[1].value;
    value2 = Input[2].value;

    vec3 normalPrev, binormalPrev, normalNext, binormalNext;

    if (Input[0].value == -1 && Input[3].value == -1)
    {
        pos0 = pos1 = Input[1].worldSpaceCoordinate;
        pos2 = pos3 = Input[2].worldSpaceCoordinate;
    }
    else if (Input[0].value == -1)
    {
        pos0 = pos1 = Input[1].worldSpaceCoordinate;
        pos2 = Input[2].worldSpaceCoordinate;
        pos3 = Input[3].worldSpaceCoordinate;
    }
    else if (Input[3].value == -1)
    {
        pos0 = Input[0].worldSpaceCoordinate;
        pos1 = Input[1].worldSpaceCoordinate;
        pos2 = pos3 = Input[2].worldSpaceCoordinate;
    }
    else
    {
        pos0 = Input[0].worldSpaceCoordinate;
        pos1 = Input[1].worldSpaceCoordinate;
        pos2 = Input[2].worldSpaceCoordinate;
        pos3 = Input[3].worldSpaceCoordinate;
    }

    calculateRayBasis(pos0, pos2, normalPrev, binormalPrev);
    calculateRayBasis(pos1, pos3, normalNext, binormalNext);


    // Generate tube.

    int tube_segments = 8;
    float angle_t = 360. / float(tube_segments);

    vec3 tube_positions[8];

    for (int t = 0; t <= tube_segments; t++)
    {
        float angle = radians(angle_t * t);
        float cosi = cos(angle) * tubeRadius;
        float sini = sin(angle) * tubeRadius;

        vec3 prev_world_pos = pos1 + cosi * normalPrev + sini * binormalPrev;
        vec3 next_world_pos = pos2 + cosi * normalNext + sini * binormalNext;

        Output.valuePS = value1;
        gl_Position = mvpMatrix * vec4(prev_world_pos,1);
        Output.normalPS = normalize(prev_world_pos - pos1);
        Output.worldPos = prev_world_pos;

        EmitVertex();

        Output.valuePS = value2;
        gl_Position = mvpMatrix * vec4(next_world_pos,1);
        Output.normalPS = normalize(next_world_pos - pos2);
        Output.worldPos = next_world_pos;

        EmitVertex();

        tube_positions[t] = next_world_pos;
    }

    EndPrimitive();

    if (pos2 == pos3 || pos0 == pos1 || startPrimitive)
    {
        vec3 ray_dir = vec3(0);
        vec3 next_world_mid = vec3(0);
        vec3 normalEnd = vec3(0);
        vec3 binormalEnd = vec3(0);
         if (pos2 == pos3 && !startPrimitive)
         {
            ray_dir = normalize(pos2 - pos1);
            next_world_mid = pos2;
            normalEnd = normalNext;
            binormalEnd = binormalNext;
         }
         if (pos0 == pos1 || startPrimitive)
         {
            ray_dir = normalize(pos1 - pos2);
            next_world_mid = pos1;
            normalEnd = normalPrev;
            binormalEnd = binormalPrev;
         }

        int ver_num = 0;

        angle_t = 360.0 / tube_segments;

        //#pragma unroll
        for (int t = 0; t < tube_segments; ++t)
        {
            float angle = radians(angle_t * t);
            float cosi = cos(angle) * tubeRadius;
            float sini = sin(angle) * tubeRadius;

            vec3 next_world_pos_0 = next_world_mid + cosi * normalEnd
                    + sini * binormalEnd;

            angle = radians(angle_t * (t + 1));
            cosi = cos(angle) * tubeRadius;
            sini = sin(angle) * tubeRadius;

            vec3 next_world_pos_1 = next_world_mid + cosi * normalEnd
                    + sini * binormalEnd;

            Output.normalPS = normalize(next_world_pos_0 - next_world_mid);
            gl_Position = mvpMatrix * vec4(next_world_pos_0,1);
            Output.worldPos = next_world_pos_0;
            EmitVertex();

            Output.normalPS = ray_dir;
            Output.worldPos = next_world_mid;
            gl_Position = mvpMatrix * vec4(next_world_mid,1);
            EmitVertex();

            Output.normalPS = normalize(next_world_pos_1 - next_world_mid);
            gl_Position = mvpMatrix * vec4(next_world_pos_1,1);
            Output.worldPos = next_world_pos_1;
            EmitVertex();

            EndPrimitive();

        }
    }
}


shader GSTubeShadow(in VStoGS Input[], out GStoFS Output)
{
    // set value of all to zero
    Output.valuePS = 0;

    if (Input[1].value == -1 || Input[2].value == -1)
    {
        return;
    }
    //    o ----o______o-----o
    vec3 pos0, pos1, pos2, pos3;

    float value1, value2;

    // the scalar values at points pos1 and pos2
    value1 = Input[1].value;
    value2 = Input[2].value;

    // determine the positions that have to be used
    if (Input[0].value == -1 && Input[3].value == -1)
    {
        pos0 = pos1 = Input[1].worldSpaceCoordinate;
        pos2 = pos3 = Input[2].worldSpaceCoordinate;
    }
    else if (Input[0].value == -1)
    {
        pos0 = pos1 = Input[1].worldSpaceCoordinate;
        pos2 = Input[2].worldSpaceCoordinate;
        pos3 = Input[3].worldSpaceCoordinate;
    }
    else if (Input[3].value == -1)
    {
        pos0 = Input[0].worldSpaceCoordinate;
        pos1 = Input[1].worldSpaceCoordinate;
        pos2 = pos3 = Input[2].worldSpaceCoordinate;
    }
    else
    {
        pos0 = Input[0].worldSpaceCoordinate;
        pos1 = Input[1].worldSpaceCoordinate;
        pos2 = Input[2].worldSpaceCoordinate;
        pos3 = Input[3].worldSpaceCoordinate;
    }

    // we only need the normal parallel to the x/y plane
    vec3 normalPrev, normalNext;
    // approximated tangent of pos1
    const vec3 prevDir = normalize(pos2 - pos0);
    // approximated tangent of pos2
    const vec3 nextDir = normalize(pos3 - pos1);

    // compute normals of tangents
    normalPrev = cross(prevDir, vec3(0,0,1));
    normalNext = cross(nextDir, vec3(0,0,1));

    // set all normals of output to zero
    Output.normalPS = normalize(vec3(0,0,0));

    // and calculate the boundaries of the projected quad in x/y plane
    Output.worldPos = pos1 - normalPrev * tubeRadius;
    gl_Position = mvpMatrix * vec4(Output.worldPos, 1);
    EmitVertex();

    Output.worldPos = pos1 + normalPrev * tubeRadius;
    gl_Position = mvpMatrix * vec4(Output.worldPos, 1);
    EmitVertex();

    Output.worldPos = pos2 - normalNext * tubeRadius;
    gl_Position = mvpMatrix * vec4(Output.worldPos, 1);
    EmitVertex();

    Output.worldPos = pos2 + normalNext * tubeRadius;
    gl_Position = mvpMatrix * vec4(Output.worldPos, 1);
    EmitVertex();

    EndPrimitive();
}


/*****************************************************************************
 ***                          FRAGMENT SHADER
 *****************************************************************************/

uniform sampler1D transferFunction;

uniform float   tfMinimum;
uniform float   tfMaximum;

uniform bool normalized;

uniform vec4    shadowColor;

shader FSLine(in GStoFSLine Input, out vec4 fragColor)
{
    if (Input.valuePS == -1)
        discard;

    float t = Input.valuePS;

    if (!normalized)
    {
        t = (t - tfMinimum) / (tfMaximum - tfMinimum);
    }

    fragColor = texture(transferFunction, t);
}


/*****************************************************************************/

uniform vec3    cameraPosition;
uniform vec3    lightDirection;

void getBlinnPhongColor(in vec3 worldPos, in vec3 normalDir,
                        in vec3 ambientColor, out vec3 color)
{
    const vec3 lightColor = vec3(1,1,1);

    const vec3 kA = 0.3 * ambientColor;
    const vec3 kD = 0.5 * ambientColor;
    const float kS = 0.2;
    const float s = 10;

    const vec3 n = normalize(normalDir);
    const vec3 v = normalize(cameraPosition - worldPos);
    const vec3 l = normalize(-lightDirection); // specialCase
    const vec3 h = normalize(v + l);

    vec3 diffuse = kD * clamp(abs(dot(n,l)),0.0,1.0) * lightColor;
    vec3 specular = kS * pow(clamp(abs(dot(n,h)),0.0,1.0),s) * lightColor;

    color = kA + diffuse + specular;
}


shader FSGeom(in GStoFS Input, out vec4 fragColor)
{
    if (Input.valuePS == -1) { discard; }

    vec3 color;
    float t = Input.valuePS;

    if (!normalized)
    {
        t = (t - tfMinimum) / (tfMaximum - tfMinimum);
    }

    const vec3 ambientColor = texture(transferFunction, t).rgb;

    getBlinnPhongColor(Input.worldPos, Input.normalPS, ambientColor, color);
    fragColor = vec4(color,1);
}


shader FSShadow(in GStoFS Input, out vec4 fragColor)
{
    if (Input.valuePS == -1)
    {
        discard;
    }

    fragColor = shadowColor;
}

shader FSSimple(in GStoFS Input, out vec4 fragColor)
{
    if (Input.valuePS == -1)
    {
        discard;
    }
    vec3 color;

    float value = Input.valuePS;

    if (colorMode == 1)
    {
        value = exp(Input.worldPos.z / pToWorldZParams.y + pToWorldZParams.x);
    }

    float t = (value - tfMinimum) / (tfMaximum - tfMinimum);

    vec3 ambientColor = vec3(0);
    if (colorMode == 0)
    {
        ambientColor = tubeColor;
    }
    else
    {
        ambientColor = texture(transferFunction, t).rgb;
    }

    getBlinnPhongColor(Input.worldPos, Input.normalPS, ambientColor, color);

    fragColor = vec4(color, 1);
}


/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program Line
{
    vs(420)=VSmain();
    gs(420)=GSLine() : in(lines_adjacency), out(line_strip, max_vertices = 2);
    fs(420)=FSLine();
};


program Box
{
    vs(420)=VSmain();
    gs(420)=GSBox() : in(lines_adjacency), out(triangle_strip, max_vertices = 128);
    fs(420)=FSGeom();
};


program Tube
{
    vs(420)=VSmain();
    gs(420)=GSTube() : in(lines_adjacency), out(triangle_strip, max_vertices = 128);
    fs(420)=FSGeom();
};


program TubeShadow
{
    vs(420)=VSShadow();
    gs(420)=GSTubeShadow() : in(lines_adjacency), out(triangle_strip, max_vertices = 128);
    fs(420)=FSShadow();
};


program Trajectory
{
    vs(420)=VSTrajectoryMain();
    gs(420)=GSTube() : in(lines_adjacency), out(triangle_strip,
                                                max_vertices = 128);
    fs(420)=FSSimple();
};
