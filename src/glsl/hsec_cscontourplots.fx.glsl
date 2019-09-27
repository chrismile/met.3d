/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Bianca Tost
**  Copyright 2015-2017 Marc Rautenhaus
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

// Using bindless textures
//#version 450 core
#extension GL_ARB_bindless_texture : require
#extension GL_NV_shader_buffer_load : enable

/*****************************************************************************
 ***                             CONSTANTS
 *****************************************************************************/

const float MISSING_VALUE    = -999.E9;
// maximum number of members
const int   MAX_NUM_MEMBERS = 51;
const int  BLOCKSIZE = 1024; // needs to be the same number as local_x!

// Vertical level type; see structuredgrid.h.
const int SURFACE_2D = 0;
const int PRESSURE_LEVELS_3D = 1;
const int HYBRID_SIGMA_PRESSURE_3D = 2;
const int POTENTIAL_VORTICITY_2D = 3;
const int LOG_PRESSURE_LEVELS_3D = 4;
const int AUXILIARY_PRESSURE_3D = 5;


/*****************************************************************************
 ***                             INTERFACES
 *****************************************************************************/

/*****************************************************************************
 ***                           COMPUTE SHADER
 *****************************************************************************/

uniform int       iOffset;           // grid index offsets if only a subregion
uniform int       jOffset;           //   of the grid shall be rendered

uniform float isoValue;               // isoValue for boxplot computation


// 64 bit handle can be converted to a sampler
uniform uvec2 textureHandles[MAX_NUM_MEMBERS];
uniform uvec2 gridTextureHandles[MAX_NUM_MEMBERS];

uniform sampler2D dataField;         // 2D texture holding the scalar data

layout(r8)
uniform image2D binaryMapTexture; // contour boxplot data

//uniform int   member;

shader CSbinaryMap()
{
    int lon = int(gl_GlobalInvocationID.x);
    int lat = int(gl_GlobalInvocationID.y);
    int member = int(gl_GlobalInvocationID.z);
    // Compute grid indices (i, j) of the this vertex from vertex and instance
    // ID (notes 09Feb2012).
    int i = int(lon / 2);
    int j = bool(lon & 1) ? (lat + 1) : lat;

    // Subregion shift?
    int numLons = textureSize(dataField, 0).x;
    i = (i + iOffset) % numLons;
    j = j + jOffset;

    vec4 data = texelFetch(sampler2D(gridTextureHandles[member]), ivec2(i, j),
                           0);
    float scalar = data.r;

    imageStore(layout(r8) image2D(textureHandles[member]), ivec2(i, j),
               vec4(float(scalar>isoValue), 0, 0, 0));
}

/*****************************************************************************/
/*****************************************************************************/

/* epsilon: tolerance border (max. percentage of grid points where the contour can
 * lie  outside the pair of contours it is tested against but still is accepted as
 * lying inbetween.) */
uniform float epsilon;

/*  numPairsFloat: number of contour pairs one contour needs to be tested
 *                 against stored as float value
 *  formula: from n choose r; n=#members-1 (contour pairs are chosen from
 *           set without contour to test against), r=2 */
uniform float numPairsFloat;

uniform int nlons; // Size of grid in longitude direction.
uniform int nlats; // Size of grid in latitude direction.

uniform int xBorder;  // Eastern border of the scalar field selected:
                      // westernLonIndex + min(nLonsVar, nLonsGrid)
uniform int yBorder;  // Southern border of the scalar field selected:
                      // nothernLatIndex + nLatsVar

shared float nIntersectFail[BLOCKSIZE];
shared float nUnionFail[BLOCKSIZE];

layout(r32f)
uniform image1D bandDepth; // band depth

// number of ensemble members
uniform int nMembers;

shader CSbandDepth()
{
    int lon = int(gl_LocalInvocationID.x);
    int member = int(gl_GlobalInvocationID.z);

    int textureSize = nlons * nlats;
    int blockSize = int(ceil(float(textureSize)/float(BLOCKSIZE)));

    int position = lon;
    int start_input = blockSize * lon;
    int end_input = min(blockSize * (lon + 1), textureSize);

    float value = 0.0f;
    float temp = 0.0f;
    float mValue, n0Value, n1Value = 0;
    // lon (x) and lat (y) coordinates
    int x, y = 0;
    int n0, n1, i;

    sampler2D mTexture = sampler2D(textureHandles[member]);
    sampler2D n0Texture, n1Texture;

    nIntersectFail[position] = 0.f;
    nUnionFail[position] = 0.f;


    for (n0 = 0; n0 < nMembers; n0++)
    {
        if (member != n0)
        {
            for (n1 = n0 + 1; n1 < nMembers; n1++)
            {
                if (member != n1)
                {
                  // Parallel reduction begin :
                    // Reset shared memory by filling it with zero.
                    nIntersectFail[position] = 0.0f;
                    nUnionFail[position] = 0.0f;

                    n0Texture = sampler2D(textureHandles[n0]);
                    n1Texture = sampler2D(textureHandles[n1]);

                    // Reading from input and filling shared memory.
                    for (i = start_input; i < end_input; i++)
                    {
                        // Calculate 2D coordinates from 1D coordinate.
                        y = int(floor(i / nlons));
                        x = i - nlons * y;
                        // Apply shift.
                        x = x + iOffset;
                        y = y + jOffset;
                        // Check if we are inside the chosen data field region.
                        if ((x < xBorder) && (y < yBorder))
                        {
                            // Since the binary map texture is defined to be
                            // wrap by repeating we don't need to adapt the x
                            // coordinate if we leave the texture domain.
                            mValue = texelFetch(mTexture, ivec2(x, y), 0).r;
                            n0Value = texelFetch(n0Texture, ivec2(x, y), 0).r;
                            n1Value = texelFetch(n1Texture, ivec2(x, y), 0).r;
                            // Test if intersection of n0 and n1 fails to lie in
                            // m.
                            if (min(n0Value, n1Value) > mValue)
                            {
                                nIntersectFail[position] =
                                        nIntersectFail[position] + 1.f;
                            }
                            // Test if m fails to lie in union of n0 and n1.
                            if (mValue > max(n0Value, n1Value))
                            {
                                nUnionFail[position] =
                                        nUnionFail[position] + 1.f;
                            }
                        }

                    }
                    barrier();
                    // Reduce values by reading from shared memory.
                    for (i = 1; i < BLOCKSIZE; i *= 2)
                    {
                        if (mod(position, i * 2) == 0)
                        {
                            // Sum up intersection fails.
                            nIntersectFail[position] +=
                                    nIntersectFail[position + i];
                            // Sum up union fails.
                            nUnionFail[position] = nUnionFail[position]
                                                  + nUnionFail[position+i];
                        }
                        barrier();
                    }
                  // : Parallel reduction end.
                    value = value + float((nIntersectFail[0] <= epsilon)
                                           && (nUnionFail[0] <= epsilon));
                }
            }
        }
    }

    if(position == 0)
    {
        // Combine band depth values with member grid indices to preserve
        // connection between member and value after sorting the array.
        value = float(member) * 10.f + value / numPairsFloat;
        imageStore(bandDepth, member, vec4(value, 0.f, 0.f, 0.f));
    }

}

/*****************************************************************************/
/*****************************************************************************/

/*  nPairs: number of contour pairs one contour needs to be tested against
 *          stored as integer value.
 *  formula: from n choose r; n=#members-1 (contour pairs are chosen from
 *           set without contour to test against), r=2 */
uniform int nPairs;

layout(r32f)
uniform image2D epsilonMatrix; // epsilon matrix

shader CSdefaultEpsilon()
{
    int lon = int(gl_LocalInvocationID.x);
    int position = lon;

    int mem = int(gl_WorkGroupID.x);
    int m0 = int(gl_WorkGroupID.y);
    int m1 =  int(gl_WorkGroupID.z);
    
    // We start more threads than we need so skip threads which don't belong to
    // a part of the matrix to be filled. For more details read explanation
    // below.
    if (m1 < (nMembers - m0 - 2))
    {
        // The matrix is filled "member-memberpair"-combinations but without
        // duplicates of combinations regardless the order and without
        // duplicates of members in a combination.
        // As an example one row could contain the values of following
        // combinations:
        // (1,0,2),(1,0,3),(1,0,4),(1,2,3),(1,2,4),(1,3,4) with (mem,m0,m1).
        // Thus with increasing m0 we "lose" one combination per increment.
        // This lost can be calculated by the formula:
        //    sum(n) = (n-1)*(n/2) = ((n-1)*n)/2
        // Considering this we can assume to take a "step" of (nMembers-2) per
        // increment of m0 and to get the correct position in the matrix row
        // we reduce the position by sum(m0).
        int matpos = ((nMembers - 2) * m0) - ((m0 - 1) * m0) / 2;
        // Consider m1 in row position.
        matpos = matpos + m1;

        // Compute member index of second member of the pair by considering it
        // to be always greater than the index of the first member of the pair
        // (cf. example in explanation above).
        m1 = m0 + 1 + m1;

        // Skip current member. Skipping needs to affect all following member
        // indices.
        if (m0 >= mem)
        {
            m0++;
        }
        if (m1 >= mem)
        {
            m1++;
        }

        int textureSize = nlons * nlats;
        float nlonslats = float(nlons * nlats);
        int blockSize = int(ceil( float(textureSize) / float(BLOCKSIZE)));

        int start_input = blockSize * position;
        int end_input = min(blockSize * (position + 1), textureSize);

        float value = 0.f;
        float temp = 0.f;
        float mValue, m0Value, m1Value = 0;
        int x, y = 0;

// Begin of parallel reduction.
        // Reset shared memory.
        nIntersectFail[position] = 0.0f;
        nUnionFail[position] = 0.0f;

        sampler2D mTexture = sampler2D(textureHandles[mem]);
        sampler2D m0Texture = sampler2D(textureHandles[m0]);
        sampler2D m1Texture = sampler2D(textureHandles[m1]);

        // Reading from input and filling shared memory.
        for (int i = start_input; i < end_input; i++)
        {
            // Calculate 2D coordinates from 1D coordinate.
            y = int(floor(i / nlons));
            x = i - nlons * y;
            // Shift.
            x = x + iOffset;
            y = y + jOffset;
            // Check if point lies in the region selected.
            if ((x < xBorder) && (y < yBorder))
            {
                // Since the binary map texture is defined to be wrap by
                // repeating we don't need to adapt the x coordinate if we
                // leave the texture.
                mValue = texelFetch(mTexture, ivec2(x, y), 0).r;
                m0Value = texelFetch(m0Texture, ivec2(x, y), 0).r;
                m1Value = texelFetch(m1Texture, ivec2(x, y), 0).r;
                // Test if intersection of m0 and m1 fails to lie in m.
                if (min(m0Value, m1Value) > mValue)
                {
                    nIntersectFail[position] = nIntersectFail[position] + 1.f;
                }
                // Test if m fails to lie in union of m0 and m1.
                if (mValue > max(m0Value, m1Value))
                {
                    nUnionFail[position] = nUnionFail[position] + 1.f;
                }
            }
        }
        barrier();
        // Reduce values while reading from shared memory.
        for (int i = 1; i < BLOCKSIZE; i *= 2)
        {
            if (mod(position, i * 2) == 0)
            {
                // Reduce intersection fails.
                nIntersectFail[position] += nIntersectFail[position + i];
                // Reduce union fails.
                nUnionFail[position] = nUnionFail[position]
                        + nUnionFail[position + i];
            }
            barrier();
        }
// End of parallel reduction.

        barrier();
        // Normalize result of parallel reduction and write it to the texture
        // holding the matrix. (Only one thread needs to write the result per
        // matrix entry.)
        if (position == 0)
        {
            float temp = (max(nIntersectFail[0], nUnionFail[0]) / nlonslats);
            imageStore(epsilonMatrix, ivec2(matpos, mem),
                       vec4(temp, 0.f, 0.f, 0.f));
        }

    }
}

/*****************************************************************************/
/*****************************************************************************/

layout(rgba32f)
uniform image2D contourBoxplotTexture; // Contour boxplot data.

uniform sampler1D bandDepthSampler; // 1D texture holding the band depth values
                                    // [member*10+value]
uniform int   numInnerMembers; // number of members forming the inner band
uniform int   nonZeroMembers;  // number of members with a non-zero band depth
                               // values
uniform float isoValueTimes2;  // isovalue * 2

shader CSmain()
{
    int lon = int(gl_GlobalInvocationID.x);
    int lat = int(gl_GlobalInvocationID.y);
    // Compute grid indices (i, j) of the this vertex from vertex and instance
    // ID (notes 09Feb2012).
    int i = lon;
    int j = lat;

    // Subregion shift?
    int numLons = imageSize(contourBoxplotTexture).x;
    i = (i + iOffset) % numLons;
    j = j + jOffset;

    float value = 0.0f;
    float outerMin, outerMax, innerMin, innerMax = 0.f;

    // Initialize min and max with the value of the first member in bandDepth
    // ( = median contour member).
    value = texelFetch(bandDepthSampler, 0, 0).r;
    innerMin = texelFetch(sampler2D(gridTextureHandles[int(floor(value / 10.f))]),
            ivec2(i, j), 0).r;
    innerMax = innerMin;
    outerMin = innerMin;
    outerMax = outerMin;

    // Search for smallest and greatest value in members forming the inner
    // contour boxplot.
    for (int m = 1; m < numInnerMembers; m++)
    {
        value = texelFetch(bandDepthSampler, m, 0).r;
        value = texelFetch(sampler2D(gridTextureHandles[int(floor(value / 10.f))]),
                ivec2(i, j), 0).r;
        innerMin = min(innerMin, value);
        innerMax = max(innerMax, value);
    }

    // Search for smallest and greatest value in members forming the outer
    // contour boxplot.
    for (int m = 1; m < nonZeroMembers; m++)
    {
        value = texelFetch(bandDepthSampler, m, 0).r;
        value = texelFetch(sampler2D(gridTextureHandles[int(floor(value / 10.f))]),
                ivec2(i, j), 0).r;
        outerMin = min(outerMin, value);
        outerMax = max(outerMax, value);
    }

    // Calculate "mirrored" mins: min_new = -min_old + isoValue * 2 .
    // (cf. plotcollection.h)
    innerMin = -innerMin + isoValueTimes2 ;
    outerMin = -outerMin + isoValueTimes2 ;

    imageStore(contourBoxplotTexture, ivec2(i, j),
               vec4(innerMin, innerMax, outerMin, outerMax));
}

/*****************************************************************************
 ***                             PROGRAMS
 *****************************************************************************/

program ComputeBinaryMap
{
    cs(430)=CSbinaryMap(): in(local_size_x = 1, local_size_y = 1, local_size_z = 1);
};

program ComputeBandDepth
{
    cs(430)=CSbandDepth(): in(local_size_x = 1024, local_size_y = 1, local_size_z = 1);
};

program ComputeDefaultEpsilon
{
    cs(430)=CSdefaultEpsilon(): in(local_size_x = 1024, local_size_y = 1, local_size_z = 1);
};

program ComputeContourBoxplot
{
    cs(430)=CSmain(): in(local_size_x = 1, local_size_y = 1, local_size_z = 1);
};

