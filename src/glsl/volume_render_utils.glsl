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

// - This file contains multiple functions to shade and render isosurface on volumes
// - required files:
//     o volume_sample_utils.glsl
// - required uniforms:
//
// uniform int     bisectionSteps;
// uniform bool    isoEnables[MAX_ISOSURFACES];
// uniform float   isoValues[MAX_ISOSURFACES];
// uniform vec4    isoColors[MAX_ISOSURFACES];
// uniform int     isoColorModes[MAX_ISOSURFACES];
// uniform int     lightingMode;
// uniform vec3    lightDirection;
// uniform float   ambientCoeff;
// uniform float   diffuseCoeff;
// uniform float   specularCoeff;
// uniform float   shininessCoeff;
// uniform vec4    shadowColor;
//
// and all uniforms from volume_sample_utils.glsl


/**
  Blinn-Phong-similar lighting implementation. Lighting can be computed either
  double-sided in the light direction, or single-sided from the light direction.
  In the single-sided mode, an optional head light can be added. The intensity
  of this head light is scaled with the cosine between the light direction and
  the view direction to avoid over exposure if the scene if viewed from the
  light direction.

  References: Engel et al., Real time volume graphics, 2006, Ch 5.4
              Akinene-Moeller et al., Real-time rendering, 3rd ed., 2008, Ch 5.5
 */
vec3 blinnPhongColour(in vec3 pos, in vec3 normal, in vec3 colour)
{
    // View vector, pointing toward the camera.
    vec3 v = normalize(cameraPosition - pos);

    // Surface normal vector, pointing away from the surface.
    vec3 n = normalize(normal);

    // Material properties.
    vec3  Ka = ambientCoeff * colour;
    vec3  Kd = diffuseCoeff * colour;
    float Ks = specularCoeff; // specular term takes light colour
    float m  = shininessCoeff;

    // Add the ambient term to the resulting shading colour.
    vec3 shadingColour = Ka;

    if (lightingMode == 0)
    {
        // Double-sided lighting from light direction.
        // ===========================================

        // Light direction l, pointing toward the light source.
        vec3 l;
        if (shadowRay) l = vec3(0., 0., 1.);
        else l = normalize(-lightDirection);

        // The colour of the light is white.
        vec3 lightColour = vec3(1., 1., 1.);

        // Half vector, mid-way between view and light directions.
        vec3 h = normalize(v + l);

        // abs(dot()) used for double-sided lighting.
        float diffuseLight = clamp(abs(dot(n, l)), 0., 1.);
        vec3 diffuse = Kd * lightColour * diffuseLight;

        float specularLight = max(0., pow(clamp(abs(dot(n, h)), 0., 1.), m));
        vec3 specular = Ks * lightColour * specularLight;

        shadingColour += diffuse + specular;
    }
    else
    {
        // Single-sided lighting with optional additional light from the
        // view direction (if lightingMode == 2).
        // =============================================================
        int numLightSources = lightingMode;

        vec3 l[2];
        vec3 lightColour[2];
        if (shadowRay)
        {
            numLightSources = 1;
            l[0] = vec3(0., 0., 1.);
            lightColour[0] = vec3(1., 1., 1.);
        }
        else
        {
            // First light from light direction, second from head light.
            l[0] = normalize(-lightDirection);
            l[1] = v;

            lightColour[0] = vec3(1., 1., 1.);
            // Scale the head light according to cos( light dir, view dir).
            lightColour[1] = lightColour[0] * 1. - clamp(dot(l[0], l[1]), 0., 1.);
        }

        // For each light source add diffuse and specular terms:
        // =====================================================

        for (int i = 0; i < numLightSources; i++)
        {
            // Half vector, mid-way between view and light directions.
            vec3 h = normalize(v + l[i]);

            float diffuseLight = clamp(dot(n, l[i]), 0., 1.);
            vec3 diffuse = Kd * lightColour[i] * diffuseLight;

            float specularLight = max(0., pow(clamp(dot(n, h), 0., 1.), m));
            vec3 specular = Ks * lightColour[i] * specularLight;

            shadingColour += diffuse + specular;
        }
    }

    return shadingColour;
}


// Compute the isosurface color according to the given options
vec4 getSurfaceColor(in vec3 rayPosition,
                     in vec3 gradient,
                     in float scalar, // scalar value (for transfer function)
                     in int i) // isovalue index
{
    // if isovalue not enabled, skip this isosurface and return color zero
    if(!isoEnables[i])
    {
        return vec4(0);
    }

    // Shadow mode: Alpha of shadow is determined by alpha of isosurface colour,
    // multiplied by alpha value of shadow colour to enable user to control
    // lightness of shadow.
    if (shadowRay)
    {
        return vec4(shadowColor.rgb, isoColors[i].a * shadowColor.a);
    }

    if (isoColorModes[i] == TRANSFER_FUNC_COLOUR)
    {
        vec3 diffuseColor = vec3(0.);
        // If tfMaximum == tfMinimum skip sampling of transfer texture.
        // This is used by the CPU code to signal that no valid transfer
        // function can be bound.
        if (dataExtent.tfMaximum != dataExtent.tfMinimum)
        {
            float t = (scalar - dataExtent.tfMinimum)
                          / (dataExtent.tfMaximum - dataExtent.tfMinimum);
            diffuseColor = texture(transferFunction, t).rgb;
        }
        return vec4(blinnPhongColour(rayPosition, -gradient, diffuseColor),
                    isoColors[i].a);
    }
    else if (isoColorModes[i] == TRANSFER_FUNC_SHADINGVAR)
    {
        float scalarShV = sampleShadingDataAtPos(rayPosition);
        vec3 diffuseColor = vec3(0.);
        if (dataExtentShV.tfMaximum != dataExtentShV.tfMinimum)
        {
            float t = (scalarShV - dataExtentShV.tfMinimum)
                          / (dataExtentShV.tfMaximum - dataExtentShV.tfMinimum);
            diffuseColor = texture(transferFunctionShV, t).rgb;
        }
        return vec4(blinnPhongColour(rayPosition, -gradient, diffuseColor),
                    isoColors[i].a);
    }
    else if (isoColorModes[i] == TRANSFER_FUNC_SHADINGVAR_MAXNB)
    {
        float scalarShV = sampleShadingDataAtPos_maxNeighbour(rayPosition);
        vec3 diffuseColor = vec3(0.);
        if (dataExtentShV.tfMaximum != dataExtentShV.tfMinimum)
        {
            float t = (scalarShV - dataExtentShV.tfMinimum)
                          / (dataExtentShV.tfMaximum - dataExtentShV.tfMinimum);
            diffuseColor = texture(transferFunctionShV, t).rgb;
        }
        return vec4(blinnPhongColour(rayPosition, -gradient, diffuseColor),
                    isoColors[i].a);
    }
    else
    {
        return vec4(blinnPhongColour(rayPosition, -gradient, isoColors[i].rgb),
                    isoColors[i].a);
    }
}


vec4 blendIsoSurfaces(in int crossingLevelFront, in int crossingLevelBack,
                      in vec3 rayPosition, in vec3 gradient, in float scalar)
{
    vec4 blendColor = vec4(0);

    const bool invertedCrossing = crossingLevelFront < crossingLevelBack;

    vec4 surfaceColor = vec4(0);

    if (invertedCrossing)
    {
        for (int i = crossingLevelBack - 1; i >= crossingLevelFront; --i)
        {
            surfaceColor = getSurfaceColor(rayPosition, gradient, scalar, i);

            blendColor.rgb += (1 - blendColor.a) * surfaceColor.a * surfaceColor.rgb;
            blendColor.a += (1 - blendColor.a) * surfaceColor.a;
        }
    }
    else
    {
        for (int i = crossingLevelBack; i < crossingLevelFront; ++i)
        {
            surfaceColor = getSurfaceColor(rayPosition, gradient, scalar, i);

            blendColor.rgb += (1 - blendColor.a) * surfaceColor.a * surfaceColor.rgb;
            blendColor.a += (1 - blendColor.a) * surfaceColor.a;
        }
    }

    return blendColor;
}

