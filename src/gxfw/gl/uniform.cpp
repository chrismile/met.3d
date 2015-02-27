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
*******************************************************************************/
#include "shadereffect.h"

// standard library imports

// related third party imports
#include <log4cplus/loggingmacros.h>

// local application imports
#include "util/mutil.h"


namespace GL
{

/*******************************************************************************
***                         CONSTRUCTOR / DESTRUCTOR
*******************************************************************************/

MShaderEffect::Uniform::Uniform(const GLint _location, const GLuint _index,
                                const GLenum _type, const GLuint _size,
                                const QString _name) :
    location(_location),
    index(_index),
    type(_type),
    size(_size),
    name(_name)
{
}


MShaderEffect::Uniform::~Uniform()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

QString MShaderEffect::Uniform::getName() const
{
    return name;
}


GLint MShaderEffect::Uniform::getLocation() const
{
    return location;
}


GLuint MShaderEffect::Uniform::getIndex() const
{
    return index;
}


GLuint MShaderEffect::Uniform::getSize() const
{
    return size;
}


GLenum MShaderEffect::Uniform::getType() const
{
    return type;
}


bool MShaderEffect::Uniform::setUniform(const GLenum inputType,
                                        const void* data,
                                        const GLuint count,
                                        const GLuint inputTupleSize)
{
    Q_UNUSED(inputType);
    Q_UNUSED(inputTupleSize);

    // if uniform is not active, do not perform an update
    if (location == -1)
    {
        return false;
    }

    // call gl function corresponding to uniform value
    switch (type)
    {

    case GL_FLOAT:
        glUniform1fv(location, count, static_cast<const GLfloat*>(data)); break;
    case GL_FLOAT_VEC2:
        glUniform2fv(location, count, static_cast<const GLfloat*>(data)); break;
    case GL_FLOAT_VEC3:
        glUniform3fv(location, count, static_cast<const GLfloat*>(data)); break;
    case GL_FLOAT_VEC4:
        glUniform4fv(location, count, static_cast<const GLfloat*>(data)); break;

    case GL_DOUBLE:
        glUniform1dv(location, count, static_cast<const GLdouble*>(data)); break;
    case GL_DOUBLE_VEC2:
        glUniform2dv(location, count, static_cast<const GLdouble*>(data)); break;
    case GL_DOUBLE_VEC3:
        glUniform3dv(location, count, static_cast<const GLdouble*>(data)); break;
    case GL_DOUBLE_VEC4:
        glUniform4dv(location, count, static_cast<const GLdouble*>(data)); break;

    case GL_BOOL:
    case GL_INT:
        glUniform1iv(location, count, static_cast<const GLint*>(data)); break;
    case GL_BOOL_VEC2:
    case GL_INT_VEC2:
        glUniform2iv(location, count, static_cast<const GLint*>(data)); break;
    case GL_BOOL_VEC3:
    case GL_INT_VEC3:
        glUniform3iv(location, count, static_cast<const GLint*>(data)); break;
    case GL_BOOL_VEC4:
    case GL_INT_VEC4:
        glUniform4iv(location, count, static_cast<const GLint*>(data)); break;

    case GL_UNSIGNED_INT:
        glUniform1uiv(location, count, static_cast<const GLuint*>(data)); break;
    case GL_UNSIGNED_INT_VEC2:
        glUniform2uiv(location, count, static_cast<const GLuint*>(data)); break;
    case GL_UNSIGNED_INT_VEC3:
        glUniform3uiv(location, count, static_cast<const GLuint*>(data)); break;
    case GL_UNSIGNED_INT_VEC4:
        glUniform4uiv(location, count, static_cast<const GLuint*>(data)); break;


    case GL_FLOAT_MAT2:
        glUniformMatrix2fv(location, count, FALSE, static_cast<const GLfloat*>(data)); break;
    case GL_FLOAT_MAT3:
        glUniformMatrix3fv(location, count, FALSE, static_cast<const GLfloat*>(data)); break;
    case GL_FLOAT_MAT4:
        glUniformMatrix4fv(location, count, FALSE, static_cast<const GLfloat*>(data)); break;
    case GL_FLOAT_MAT2x3:
        glUniformMatrix2x3fv(location, count, FALSE, static_cast<const GLfloat*>(data)); break;
    case GL_FLOAT_MAT2x4:
        glUniformMatrix2x4fv(location, count, FALSE, static_cast<const GLfloat*>(data)); break;
    case GL_FLOAT_MAT3x2:
        glUniformMatrix3x2fv(location, count, FALSE, static_cast<const GLfloat*>(data)); break;
    case GL_FLOAT_MAT3x4:
        glUniformMatrix3x4fv(location, count, FALSE, static_cast<const GLfloat*>(data)); break;
    case GL_FLOAT_MAT4x2:
        glUniformMatrix4x2fv(location, count, FALSE, static_cast<const GLfloat*>(data)); break;
    case GL_FLOAT_MAT4x3:
        glUniformMatrix4x3fv(location, count, FALSE, static_cast<const GLfloat*>(data)); break;

    case GL_DOUBLE_MAT2:
        glUniformMatrix2dv(location, count, FALSE, static_cast<const GLdouble*>(data)); break;
    case GL_DOUBLE_MAT3:
        glUniformMatrix3dv(location, count, FALSE, static_cast<const GLdouble*>(data)); break;
    case GL_DOUBLE_MAT4:
        glUniformMatrix4dv(location, count, FALSE, static_cast<const GLdouble*>(data)); break;
    case GL_DOUBLE_MAT2x3:
        glUniformMatrix2x3dv(location, count, FALSE, static_cast<const GLdouble*>(data)); break;
    case GL_DOUBLE_MAT2x4:
        glUniformMatrix2x4dv(location, count, FALSE, static_cast<const GLdouble*>(data)); break;
    case GL_DOUBLE_MAT3x2:
        glUniformMatrix3x2dv(location, count, FALSE, static_cast<const GLdouble*>(data)); break;
    case GL_DOUBLE_MAT3x4:
        glUniformMatrix3x4dv(location, count, FALSE, static_cast<const GLdouble*>(data)); break;
    case GL_DOUBLE_MAT4x2:
        glUniformMatrix4x2dv(location, count, FALSE, static_cast<const GLdouble*>(data)); break;
    case GL_DOUBLE_MAT4x3:
        glUniformMatrix4x3dv(location, count, FALSE, static_cast<const GLdouble*>(data)); break;

    case GL_SAMPLER_1D:
    case GL_SAMPLER_2D:
    case GL_SAMPLER_3D:
    case GL_SAMPLER_CUBE:
    case GL_SAMPLER_1D_SHADOW:
    case GL_SAMPLER_2D_SHADOW:
    case GL_SAMPLER_1D_ARRAY:
    case GL_SAMPLER_2D_ARRAY:
    case GL_SAMPLER_1D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_ARRAY_SHADOW:
    case GL_SAMPLER_2D_MULTISAMPLE:
    case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_SAMPLER_CUBE_SHADOW:
    case GL_SAMPLER_BUFFER:
    case GL_SAMPLER_2D_RECT:
    case GL_SAMPLER_2D_RECT_SHADOW:
    case GL_INT_SAMPLER_1D:
    case GL_INT_SAMPLER_2D:
    case GL_INT_SAMPLER_3D:
    case GL_INT_SAMPLER_CUBE:
    case GL_INT_SAMPLER_1D_ARRAY:
    case GL_INT_SAMPLER_2D_ARRAY:
    case GL_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_INT_SAMPLER_BUFFER:
    case GL_INT_SAMPLER_2D_RECT:
    case GL_UNSIGNED_INT_SAMPLER_1D:
    case GL_UNSIGNED_INT_SAMPLER_2D:
    case GL_UNSIGNED_INT_SAMPLER_3D:
    case GL_UNSIGNED_INT_SAMPLER_CUBE:
    case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
    case GL_UNSIGNED_INT_SAMPLER_BUFFER:
    case GL_UNSIGNED_INT_SAMPLER_2D_RECT:
    case GL_IMAGE_1D:
    case GL_IMAGE_2D:
    case GL_IMAGE_3D:
    case GL_IMAGE_2D_RECT:
    case GL_IMAGE_CUBE:
    case GL_IMAGE_BUFFER:
    case GL_IMAGE_1D_ARRAY:
    case GL_IMAGE_2D_ARRAY:
    case GL_IMAGE_2D_MULTISAMPLE:
    case GL_IMAGE_2D_MULTISAMPLE_ARRAY:
    case GL_INT_IMAGE_1D:
    case GL_INT_IMAGE_2D:
    case GL_INT_IMAGE_3D:
    case GL_INT_IMAGE_2D_RECT:
    case GL_INT_IMAGE_CUBE:
    case GL_INT_IMAGE_BUFFER:
    case GL_INT_IMAGE_1D_ARRAY:
    case GL_INT_IMAGE_2D_ARRAY:
    case GL_INT_IMAGE_2D_MULTISAMPLE:
    case GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
    case GL_UNSIGNED_INT_IMAGE_1D:
    case GL_UNSIGNED_INT_IMAGE_2D:
    case GL_UNSIGNED_INT_IMAGE_3D:
    case GL_UNSIGNED_INT_IMAGE_2D_RECT:
    case GL_UNSIGNED_INT_IMAGE_CUBE:
    case GL_UNSIGNED_INT_IMAGE_BUFFER:
    case GL_UNSIGNED_INT_IMAGE_1D_ARRAY:
    case GL_UNSIGNED_INT_IMAGE_2D_ARRAY:
    case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE:
    case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY:
        glUniform1iv(location, count, static_cast<const GLint*>(data)); break;

    case GL_UNSIGNED_INT_ATOMIC_COUNTER: return "atomic_uint";
        glUniform1uiv(location, count, static_cast<const GLuint*>(data)); break;

    default:
        return false;
    }

    return true;
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

const char* MShaderEffect::Uniform::glEnumToSring(GLenum type) const
{
    switch (type)
    {
    case GL_FLOAT: return "float";
    case GL_FLOAT_VEC2: return "vec2";
    case GL_FLOAT_VEC3: return "vec3";
    case GL_FLOAT_VEC4: return "vec4";
    case GL_DOUBLE: return "double";
    case GL_DOUBLE_VEC2: return "dvec2";
    case GL_DOUBLE_VEC3: return "dvec3";
    case GL_DOUBLE_VEC4: return "dvec4";
    case GL_INT: return "int";
    case GL_INT_VEC2: return "ivec2";
    case GL_INT_VEC3: return "ivec3";
    case GL_INT_VEC4: return "ivec4";
    case GL_UNSIGNED_INT: return "uint";
    case GL_UNSIGNED_INT_VEC2: return "uvec2";
    case GL_UNSIGNED_INT_VEC3: return "uvec3";
    case GL_UNSIGNED_INT_VEC4: return "uvec4";
    case GL_BOOL: return "bool";
    case GL_BOOL_VEC2: return "bvec2";
    case GL_BOOL_VEC3: return "bvec3";
    case GL_BOOL_VEC4: return "bvec4";
    case GL_FLOAT_MAT2: return "mat2";
    case GL_FLOAT_MAT3: return "mat3";
    case GL_FLOAT_MAT4: return "mat4";
    case GL_FLOAT_MAT2x3: return "mat2x3";
    case GL_FLOAT_MAT2x4: return "mat2x4";
    case GL_FLOAT_MAT3x2: return "mat3x2";
    case GL_FLOAT_MAT3x4: return "mat3x4";
    case GL_FLOAT_MAT4x2: return "mat4x2";
    case GL_FLOAT_MAT4x3: return "mat4x3";
    case GL_DOUBLE_MAT2: return "dmat2";
    case GL_DOUBLE_MAT3: return "dmat3";
    case GL_DOUBLE_MAT4: return "dmat4";
    case GL_DOUBLE_MAT2x3: return "dmat2x3";
    case GL_DOUBLE_MAT2x4: return "dmat2x4";
    case GL_DOUBLE_MAT3x2: return "dmat3x2";
    case GL_DOUBLE_MAT3x4: return "dmat3x4";
    case GL_DOUBLE_MAT4x2: return "dmat4x2";
    case GL_DOUBLE_MAT4x3: return "dmat4x3";
    case GL_SAMPLER_1D: return "sampler1D";
    case GL_SAMPLER_2D: return "sampler2D";
    case GL_SAMPLER_3D: return "sampler3D";
    case GL_SAMPLER_CUBE: return "samplerCube";
    case GL_SAMPLER_1D_SHADOW: return "sampler1DShadow";
    case GL_SAMPLER_2D_SHADOW: return "sampler2DShadow";
    case GL_SAMPLER_1D_ARRAY: return "sampler1DArray";
    case GL_SAMPLER_2D_ARRAY: return "sampler2DArray";
    case GL_SAMPLER_1D_ARRAY_SHADOW: return "sampler1DArrayShadow";
    case GL_SAMPLER_2D_ARRAY_SHADOW: return "sampler2DArrayShadow";
    case GL_SAMPLER_2D_MULTISAMPLE: return "sampler2DMS";
    case GL_SAMPLER_2D_MULTISAMPLE_ARRAY: return "sampler2DMSArray";
    case GL_SAMPLER_CUBE_SHADOW: return "samplerCubeShadow";
    case GL_SAMPLER_BUFFER: return "samplerBuffer";
    case GL_SAMPLER_2D_RECT: return "sampler2DRect";
    case GL_SAMPLER_2D_RECT_SHADOW: return "sampler2DRectShadow";
    case GL_INT_SAMPLER_1D: return "isampler1D";
    case GL_INT_SAMPLER_2D: return "isampler2D";
    case GL_INT_SAMPLER_3D: return "isampler3D";
    case GL_INT_SAMPLER_CUBE: return "isamplerCube";
    case GL_INT_SAMPLER_1D_ARRAY: return "isampler1DArray";
    case GL_INT_SAMPLER_2D_ARRAY: return "isampler2DArray";
    case GL_INT_SAMPLER_2D_MULTISAMPLE: return "isampler2DMS";
    case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY: return "isampler2DMSArray";
    case GL_INT_SAMPLER_BUFFER: return "isamplerBuffer";
    case GL_INT_SAMPLER_2D_RECT: return "isampler2DRect";
    case GL_UNSIGNED_INT_SAMPLER_1D: return "usampler1D";
    case GL_UNSIGNED_INT_SAMPLER_2D: return "usampler2D";
    case GL_UNSIGNED_INT_SAMPLER_3D: return "usampler3D";
    case GL_UNSIGNED_INT_SAMPLER_CUBE: return "usamplerCube";
    case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY: return "usampler2DArray";
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY: return "usampler2DArray";
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE: return "usampler2DMS";
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY: return "usampler2DMSArray";
    case GL_UNSIGNED_INT_SAMPLER_BUFFER: return "usamplerBuffer";
    case GL_UNSIGNED_INT_SAMPLER_2D_RECT: return "usampler2DRect";
    case GL_IMAGE_1D: return "image1D";
    case GL_IMAGE_2D: return "image2D";
    case GL_IMAGE_3D: return "image3D";
    case GL_IMAGE_2D_RECT: return "image2DRect";
    case GL_IMAGE_CUBE: return "imageCube";
    case GL_IMAGE_BUFFER: return "imageBuffer";
    case GL_IMAGE_1D_ARRAY: return "image1DArray";
    case GL_IMAGE_2D_ARRAY: return "image2DArray";
    case GL_IMAGE_2D_MULTISAMPLE: return "image2DMS";
    case GL_IMAGE_2D_MULTISAMPLE_ARRAY: return "image2DMSArray";
    case GL_INT_IMAGE_1D: return "iimage1D";
    case GL_INT_IMAGE_2D: return "iimage2D";
    case GL_INT_IMAGE_3D: return "iimage3D";
    case GL_INT_IMAGE_2D_RECT: return "iimage2DRect";
    case GL_INT_IMAGE_CUBE: return "iimageCube";
    case GL_INT_IMAGE_BUFFER: return "iimageBuffer";
    case GL_INT_IMAGE_1D_ARRAY: return "iimage1DArray";
    case GL_INT_IMAGE_2D_ARRAY: return "iimage2DArray";
    case GL_INT_IMAGE_2D_MULTISAMPLE: return "iimage2DMS";
    case GL_INT_IMAGE_2D_MULTISAMPLE_ARRAY: return "iimage2DMSArray";
    case GL_UNSIGNED_INT_IMAGE_1D: return "uimage1D";
    case GL_UNSIGNED_INT_IMAGE_2D: return "uimage2D";
    case GL_UNSIGNED_INT_IMAGE_3D: return "uimage3D";
    case GL_UNSIGNED_INT_IMAGE_2D_RECT: return "uimage2DRect";
    case GL_UNSIGNED_INT_IMAGE_CUBE: return "uimageCube";
    case GL_UNSIGNED_INT_IMAGE_BUFFER: return "uimageBuffer";
    case GL_UNSIGNED_INT_IMAGE_1D_ARRAY: return "uimage1DArray";
    case GL_UNSIGNED_INT_IMAGE_2D_ARRAY: return "uimage2DArray";
    case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE: return "uimage2DMS";
    case GL_UNSIGNED_INT_IMAGE_2D_MULTISAMPLE_ARRAY: return "uimage2DMSArray";
    case GL_UNSIGNED_INT_ATOMIC_COUNTER: return "atomic_uint";
    default: return "unknown";
    }
}

} // namespace GL
