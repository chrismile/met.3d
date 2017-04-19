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
#include <stdexcept>
#include <QVector2D>
#include <QVector3D>
#include <QVector4D>
#include <QColor>
#include <QMatrix4x4>

// local application imports
#include "util/mutil.h"


namespace GL
{

/*******************************************************************************
 ***                         SET UNIFORM VALUE METHODS
 *******************************************************************************/

//=======
// BOOL1
//=======

void MShaderEffect
::setUniformValue(const QString name, const GLboolean x)
{
    // if program is invalid -> do not set uniforms
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    const GLint value = static_cast<const GLint>(x);
    uniform->setUniform(GL_BOOL, &value, 1, 1);
}


//=======
// BOOL2
//=======

void MShaderEffect
::setUniformValue(const QString name, const GLboolean x,
                  const GLboolean y)
{
    // if program is invalid -> do not set uniforms
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLint values[] = {static_cast<int>(x),
                      static_cast<int>(y)};

    uniform->setUniform(GL_BOOL_VEC2, values, 1, 2);
}


//=======
// BOOL3
//=======

void MShaderEffect
::setUniformValue(const QString name, const GLboolean x,
                  const GLboolean y,
                  const GLboolean z)
{
    // if program is invalid -> do not set uniforms
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLint values[] = {static_cast<int>(x),
                      static_cast<int>(y),
                      static_cast<int>(z)};

    uniform->setUniform(GL_BOOL_VEC3, values, 1, 3);
}


//=======
// BOOL4
//=======

void MShaderEffect
::setUniformValue(const QString name, const GLboolean x,
                  const GLboolean y,
                  const GLboolean z,
                  const GLboolean w)
{
    // if program is invalid -> do not set uniforms
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLint values[] = {static_cast<int>(x),
                      static_cast<int>(y),
                      static_cast<int>(z),
                      static_cast<int>(w)};

    uniform->setUniform(GL_BOOL_VEC4, values, 1, 4);

}


//======
// INT1
//======

void MShaderEffect
::setUniformValue(const QString name, const GLint x)
{
    // if program is invalid -> do not set uniforms
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    uniform->setUniform(GL_INT, &x, 1, 1);
}


void MShaderEffect
::setUniformValue(const QString name, const GLuint x)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    uniform->setUniform(GL_UNSIGNED_INT, &x, 1, 1);
}


//======
// INT2
//======

void MShaderEffect
::setUniformValue(const QString name, const GLint x,
                  const GLint y)
{
    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLint data[2] = { x, y };
    uniform->setUniform(GL_INT_VEC2, data, 1, 2);
}


void MShaderEffect
::setUniformValue(const QString name, const GLuint x,
                  const GLuint y)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLuint data[2] = { x, y };
    uniform->setUniform(GL_UNSIGNED_INT_VEC2, data, 1, 2);
}


//======
// INT3
//======

void MShaderEffect
::setUniformValue(const QString name, const GLint x,
                  const GLint y,
                  const GLint z)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLint data[3] = { x, y, z };
    uniform->setUniform(GL_INT_VEC3, data, 1, 3);

}


void MShaderEffect
::setUniformValue(const QString name, const GLuint x,
                  const GLuint y,
                  const GLuint z)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLuint data[3] = { x, y, z };
    uniform->setUniform(GL_UNSIGNED_INT_VEC3, data, 1, 3);
}


//======
// INT4
//======

void MShaderEffect
::setUniformValue(const QString name, const GLint x,
                  const GLint y,
                  const GLint z,
                  const GLint w)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLint data[4] = { x, y, z, w };
    uniform->setUniform(GL_INT_VEC4, data, 1, 4);
}


void MShaderEffect
::setUniformValue(const QString name, const GLuint x,
                  const GLuint y,
                  const GLuint z,
                  const GLuint w)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLuint data[4] = { x, y, z, w };
    uniform->setUniform(GL_UNSIGNED_INT_VEC4, data, 1, 4);
}


//========
// FLOAT1
//========

void MShaderEffect
::setUniformValue(const QString name, const GLfloat x)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    uniform->setUniform(GL_FLOAT, &x, 1, 1);
}


//========
// FLOAT2
//========

void MShaderEffect
::setUniformValue(const QString name, const QVector2D& vec2)
{
    setUniformValue(name, static_cast<GLfloat>(vec2.x()),
                          static_cast<GLfloat>(vec2.y()));
}


void MShaderEffect
::setUniformValue(const QString name, const GLfloat x,
                                           const GLfloat y)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLfloat data[2] = { x, y };
    uniform->setUniform(GL_FLOAT_VEC2, data, 1, 2);
}


//========
// FLOAT3
//========
void MShaderEffect
::setUniformValue(const QString name, const QVector3D& vec3)
{
    setUniformValue(name, static_cast<GLfloat>(vec3.x()),
                          static_cast<GLfloat>(vec3.y()),
                          static_cast<GLfloat>(vec3.z()));
}


void MShaderEffect
::setUniformValue(const QString name, const GLfloat x,
                                           const GLfloat y,
                                           const GLfloat z)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLfloat data[3] = { x, y, z };
    uniform->setUniform(GL_FLOAT_VEC3, data, 1, 3);
}


// =======
// FLOAT4
// =======

void MShaderEffect
::setUniformValue(const QString name, const QVector4D& vec4)
{
    setUniformValue(name, static_cast<GLfloat>(vec4.x()),
                          static_cast<GLfloat>(vec4.y()),
                          static_cast<GLfloat>(vec4.z()),
                          static_cast<GLfloat>(vec4.w()));
}


void MShaderEffect
::setUniformValue(const QString name, const GLfloat x,
                                           const GLfloat y,
                                           const GLfloat z,
                                           const GLfloat w)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLfloat data[4] = { x, y, z, w };
    uniform->setUniform(GL_FLOAT_VEC4, data, 1, 4);
}


void MShaderEffect
::setUniformValue(const QString name, const QColor& color)
{
    setUniformValue(name,
                    static_cast<GLfloat>(color.redF()),
                    static_cast<GLfloat>(color.greenF()),
                    static_cast<GLfloat>(color.blueF()),
                    static_cast<GLfloat>(color.alphaF()));
}


void MShaderEffect
::setUniformValue(const QString name, const QPoint& point)
{
    setUniformValue(name,   static_cast<GLint>(point.x()),
                            static_cast<GLint>(point.y()));
}


void MShaderEffect
::setUniformValue(const QString name, const QPointF& point)
{
    setUniformValue(name,   static_cast<GLfloat>(point.x()),
                            static_cast<GLfloat>(point.y()));
}


//========
// DOUBLE1
//========

void MShaderEffect
::setUniformValue(const QString name, const GLdouble x)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    uniform->setUniform(GL_DOUBLE, &x, 1, 1);
}


//========
// DOUBLE2
//========

void MShaderEffect
::setUniformValue(const QString name, const GLdouble x,
                  const GLdouble y)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLdouble data[2] = { x, y };
    uniform->setUniform(GL_DOUBLE_VEC2, data, 1, 2);
}


//========
// DOUBLE3
//========

void MShaderEffect
::setUniformValue(const QString name, const GLdouble x,
                                           const GLdouble y,
                                           const GLdouble z)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLdouble data[3] = { x, y, z };
    uniform->setUniform(GL_DOUBLE_VEC3, data, 1, 3);
}


// =======
// DOUBLE4
// =======

void MShaderEffect
::setUniformValue(const QString name, const GLdouble x,
                  const GLdouble y,
                  const GLdouble z,
                  const GLdouble w)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLdouble data[4] = { x, y, z, w };
    uniform->setUniform(GL_DOUBLE_VEC4, data, 1, 4);
}


//===========
// MATRIX4x4
//===========

void MShaderEffect
::setUniformValue(const QString name, const QMatrix4x4& matrix)
{
    setUniformMatrixXY(name, 1, 4, 4, GL_FLOAT_MAT4, matrix.constData());
}


//===========
// MATRIX3x3
//===========

void MShaderEffect
::setUniformValue(const QString name, const QMatrix3x3& matrix)
{
    setUniformMatrixXY(name, 1, 3, 3, GL_FLOAT_MAT3, matrix.constData());
}


//===========
// MATRIX2x2
//===========

void MShaderEffect
::setUniformValue(const QString name, const QMatrix2x2& matrix)
{  
   setUniformMatrixXY(name, 1, 2, 2, GL_FLOAT_MAT2, matrix.constData());
}


//===========
// MATRIXAxB
//===========

void MShaderEffect
::setUniformValue(const QString name, const QMatrix2x3& matrix)
{

    setUniformMatrixXY(name, 1, 2, 3, GL_FLOAT_MAT2x3, matrix.constData());
}


void MShaderEffect
::setUniformValue(const QString name, const QMatrix2x4& matrix)
{

    setUniformMatrixXY(name, 1, 2, 4, GL_FLOAT_MAT2x4, matrix.constData());
}


void MShaderEffect
::setUniformValue(const QString name, const QMatrix3x2& matrix)
{

    setUniformMatrixXY(name, 1, 3, 2, GL_FLOAT_MAT3x2, matrix.constData());
}


void MShaderEffect
::setUniformValue(const QString name, const QMatrix3x4& matrix)
{

    setUniformMatrixXY(name, 1, 3, 4, GL_FLOAT_MAT3x4, matrix.constData());
}


void MShaderEffect
::setUniformValue(const QString name, const QMatrix4x2& matrix)
{

    setUniformMatrixXY(name, 1, 4, 2, GL_FLOAT_MAT4x2, matrix.constData());
}


void MShaderEffect
::setUniformValue(const QString name, const QMatrix4x3& matrix)
{

    setUniformMatrixXY(name, 1, 4, 3, GL_FLOAT_MAT4x3, matrix.constData());
}


void MShaderEffect
::setUniformMatrixXY(const QString name, const int8_t count,
                        const int8_t cols, const int8_t rows,
                        const GLenum type, const qreal* data)
{
    if (currentProgram.second == -1) { return; }

    // QMatrix must not be transposed! --> GL_FALSE
    std::shared_ptr<Uniform>& uniform = getUniform(name);

    if (sizeof(qreal) == sizeof(GLfloat))
    {
        const GLfloat* mat = reinterpret_cast<const GLfloat*>(data);

        uniform->setUniform(type, mat, count, 1);
    }
    else
    {
        const int16_t matSize = cols * rows;
        const int16_t arrSize = matSize * count;

		// GLfloat mat[arrSize]; VLAs not supported in msvc
        GLfloat* mat = new GLfloat[arrSize];

        for (int i = 0;  i < arrSize; ++i)
        {
            mat[i] = static_cast<GLfloat>(data[i]);
        }

        uniform->setUniform(type, mat, count, 1);

		delete[] mat;
    }
}

void MShaderEffect
::setUniformMatrixXY(const QString name, const int8_t count,
                        const int8_t cols, const int8_t rows,
                        const GLenum type, const float* data)
{
    if (currentProgram.second == -1) { return; }

    // QMatrix must not be transposed! --> GL_FALSE
    std::shared_ptr<Uniform>& uniform = getUniform(name);
    const GLfloat* mat = reinterpret_cast<const GLfloat*>(data);
    uniform->setUniform(type, mat, count, 1);
}


/*******************************************************************************
 ***                       SET UNIFORM VALUE ARRAY METHODS
 *******************************************************************************/

//============
// BOOL1Array
//============

void MShaderEffect
::setUniformValueArray(const QString name, const GLboolean* data,
                       const int8_t count)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

	// GLint idata[count]; VLAs not supported in msvc
    GLint* idata = new GLint[count];

    for (int i = 0; i < count; ++i)
    {
        idata[i] = static_cast<GLint>(data[i]);
    }

    uniform->setUniform(GL_BOOL, idata, count, 1);
	delete[] idata;
}


//============
// BOOLxArray
//============

void MShaderEffect
::setUniformValueArray(const QString name, const GLboolean* data,
                       const int8_t count, const int8_t tupleSize)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

	// GLint idata[count]; VLAs not supported in msvc
    GLint* idata = new GLint[count * tupleSize];

    for (int i = 0; i < count * tupleSize; ++i)
    {
        idata[i] = static_cast<GLint>(data[i]);
    }

    GLenum type = 0;
    switch(tupleSize)
    {
    case 1:  type = GL_BOOL; break;
    case 2:  type = GL_BOOL_VEC2; break;
    case 3:  type = GL_BOOL_VEC3; break;
    case 4:  type = GL_BOOL_VEC4; break;
    }

    uniform->setUniform(type, idata, count, tupleSize);
	delete[] idata;
}


//============
// INT1Array
//============

void MShaderEffect
::setUniformValueArray(const QString name, const GLint* data,
                       const int8_t count)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    uniform->setUniform(GL_INT, data, count, 1);
}


//============
// INTxArray
//============

void MShaderEffect
::setUniformValueArray(const QString name, const GLint* data,
                       const int8_t count, const int8_t tupleSize)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLenum type = 0;
    switch(tupleSize)
    {
    case 1:  type = GL_INT; break;
    case 2:  type = GL_INT_VEC2; break;
    case 3:  type = GL_INT_VEC3; break;
    case 4:  type = GL_INT_VEC4; break;
    }

    uniform->setUniform(type, data, count, tupleSize);
}


//============
// UINT1Array
//============

void MShaderEffect
::setUniformValueArray(const QString name, const GLuint* data,
                       const int8_t count)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    uniform->setUniform(GL_UNSIGNED_INT, data, count, 1);
}


//============
// UINTxArray
//============

void MShaderEffect
::setUniformValueArray(const QString name, const GLuint* data,
                       const int8_t count, const int8_t tupleSize)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLenum type = 0;
    switch(tupleSize)
    {
    case 1:  type = GL_UNSIGNED_INT; break;
    case 2:  type = GL_UNSIGNED_INT_VEC2; break;
    case 3:  type = GL_UNSIGNED_INT_VEC3; break;
    case 4:  type = GL_UNSIGNED_INT_VEC4; break;
    }

    uniform->setUniform(type, data, count, tupleSize);
}


//============
// FLOAT1Array
//============

void MShaderEffect
::setUniformValueArray(const QString name, const GLfloat* data,
                       const int8_t count)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    uniform->setUniform(GL_FLOAT, data, count, 1);
}


//============
// FLOATxArray
//============

void MShaderEffect
::setUniformValueArray(const QString name, const GLfloat* data,
                       const int8_t count, const int8_t tupleSize)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLenum type = 0;
    switch(tupleSize)
    {
    case 1:  type = GL_FLOAT; break;
    case 2:  type = GL_FLOAT_VEC2; break;
    case 3:  type = GL_FLOAT_VEC3; break;
    case 4:  type = GL_FLOAT_VEC4; break;
    }

    uniform->setUniform(type, data, count, tupleSize);
}


//============
// DOUBLE1Array
//============

void MShaderEffect
::setUniformValueArray(const QString name, const GLdouble* data,
                       const int8_t count)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    uniform->setUniform(GL_DOUBLE, data, count, 1);
}


//============
// DOUBLExArray
//============

void MShaderEffect
::setUniformValueArray(const QString name, const GLdouble* data,
                       const int8_t count, const int8_t tupleSize)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLenum type = 0;
    switch(tupleSize)
    {
    case 1:  type = GL_DOUBLE; break;
    case 2:  type = GL_DOUBLE_VEC2; break;
    case 3:  type = GL_DOUBLE_VEC3; break;
    case 4:  type = GL_DOUBLE_VEC4; break;
    }

    uniform->setUniform(type, data, count, tupleSize);
}


//================
// MATRIXBxBArray
//================

void MShaderEffect
::setUniformValueArray(const QString name, const QMatrix4x4* data,
                       const int8_t count)
{ 
    setUniformMatrixXY(name, count, 4, 4, GL_FLOAT_MAT4, data[0].constData());
}


void MShaderEffect
::setUniformValueArray(const QString name, const QMatrix3x3* data,
                          const int8_t count)
{
    setUniformMatrixXY(name, count, 3, 3, GL_FLOAT_MAT3, data[0].constData());
}


void MShaderEffect
::setUniformValueArray(const QString name, const QMatrix2x2* data,
                          const int8_t count)
{
    setUniformMatrixXY(name, count, 2, 2, GL_FLOAT_MAT2, data[0].constData());
}


//================
// MATRIXAxBArray
//================

void MShaderEffect
::setUniformValueArray(const QString name, const QMatrix2x3* data,
                       const int8_t count)
{
    setUniformMatrixXY(name, count, 2, 3, GL_FLOAT_MAT2x3, data[0].constData());
}


void MShaderEffect
::setUniformValueArray(const QString name, const QMatrix2x4* data,
                       const int8_t count)
{
    setUniformMatrixXY(name, count, 2, 4, GL_FLOAT_MAT2x4, data[0].constData());
}


void MShaderEffect
::setUniformValueArray(const QString name, const QMatrix3x2* data,
                          const int8_t count)
{
    setUniformMatrixXY(name, count, 3, 2, GL_FLOAT_MAT3x2, data[0].constData());
}


void MShaderEffect
::setUniformValueArray(const QString name, const QMatrix3x4* data,
                          const int8_t count)
{

    setUniformMatrixXY(name, count, 3, 4, GL_FLOAT_MAT3x4, data[0].constData());
}


void MShaderEffect
::setUniformValueArray(const QString name, const QMatrix4x2* data,
                          const int8_t count)
{
    setUniformMatrixXY(name, count, 4, 2, GL_FLOAT_MAT4x2, data[0].constData());
}


void MShaderEffect
::setUniformValueArray(const QString name, const QMatrix4x3* data,
                          const int8_t count)
{
    setUniformMatrixXY(name, count, 4, 3, GL_FLOAT_MAT4x3, data[0].constData());
}


//================
// QVectorXArray
//================

void MShaderEffect
::setUniformValueArray(const QString name, const QVector4D* data,
                       const int8_t count)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

	// GLint floatData[count]; VLAs not supported in msvc
    GLfloat* floatData = new GLfloat[count * 4];

    for (int i = 0; i < count; ++i)
    {
        floatData[i * 4 + 0] = data[i].x();
        floatData[i * 4 + 1] = data[i].y();
        floatData[i * 4 + 2] = data[i].z();
        floatData[i * 4 + 3] = data[i].w();
    }

    uniform->setUniform(GL_FLOAT_VEC4, floatData, count, 4);
	delete[] floatData;
}


void MShaderEffect
::setUniformValueArray(const QString name, const QVector3D* data,
                       const int8_t count)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

	// GLint floatData[count]; VLAs not supported in msvc
    GLfloat* floatData = new GLfloat[count * 3];

    for (int i = 0; i < count; ++i)
    {
        floatData[i * 3 + 0] = data[i].x();
        floatData[i * 3 + 1] = data[i].y();
        floatData[i * 3 + 2] = data[i].z();
    }

    uniform->setUniform(GL_FLOAT_VEC3, floatData, count, 3);
	delete[] floatData;
}


void MShaderEffect
::setUniformValueArray(const QString name, const QVector2D* data,
                       const int8_t count)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

	// GLint floatData[count]; VLAs not supported in msvc
    GLfloat* floatData = new GLfloat[count * 2];

    for (int i = 0; i < count; ++i)
    {
        floatData[i * 2 + 0] = data[i].x();
        floatData[i * 2 + 1] = data[i].y();
    }

    uniform->setUniform(GL_FLOAT_VEC2, floatData, count, 2);
	delete[] floatData;
}


void MShaderEffect
::setUniformValueArray(const QString name, const QColor* data,
                       const int8_t count)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

	// GLint floatData[count]; VLAs not supported in msvc
    GLfloat* floatData = new GLfloat[count * 4];

    for (int i = 0; i < count; ++i)
    {
        floatData[i * 4 + 0] = static_cast<GLfloat>(data[i].redF());
        floatData[i * 4 + 1] = static_cast<GLfloat>(data[i].greenF());
        floatData[i * 4 + 2] = static_cast<GLfloat>(data[i].blueF());
        floatData[i * 4 + 3] = static_cast<GLfloat>(data[i].alphaF());
    }

    uniform->setUniform(GL_FLOAT_VEC4, floatData, count, 4);
	delete[] floatData;
}


void MShaderEffect
::setUniformValueArray(const QString name, const QPoint* data,
                       const int8_t count)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

	// GLint iData[count]; VLAs not supported in msvc
    GLint* iData = new GLint[count * 2];

    for (int i = 0; i < count; ++i)
    {
        iData[i * 2 + 0] = static_cast<GLint>(data[i].x());
        iData[i * 2 + 1] = static_cast<GLint>(data[i].y());
    }

    uniform->setUniform(GL_INT_VEC2, iData, count, 2);
	delete[] iData;
}


void MShaderEffect
::setUniformValueArray(const QString name, const QPointF* data,
                       const int8_t count)
{
    if (currentProgram.second == -1) { return; }

    std::shared_ptr<Uniform>& uniform = getUniform(name);

    GLfloat* floatData = new GLfloat[count * 2];

    for (int i = 0; i < count; ++i)
    {
        floatData[i * 2 + 0] = static_cast<GLfloat>(data[i].x());
        floatData[i * 2 + 1] = static_cast<GLfloat>(data[i].y());
    }

    uniform->setUniform(GL_FLOAT_VEC2, floatData, count, 2);
	delete[] floatData;
}


/*******************************************************************************
 ***                      SET UNIFORM SUBROUTINE METHODS
 *******************************************************************************/

void MShaderEffect::setUniformSubroutine(const GLenum shadertype,
                                         const std::vector<GLuint> &indices)
{
    setUniformSubroutine(shadertype, indices.size(), &indices[0]);
}


void MShaderEffect::setUniformSubroutine(const GLenum shadertype,
                                         const GLsizei count,
                                         const GLuint* indices)
{
    // get the number of current uniforms in the shader program
    const GLsizei numUniformLocations = getCurrentProgramSubroutineParam(
                GLActiveSubroutineUniformLocations, shadertype);

    if(numUniformLocations <= 0) {
        LOG4CPLUS_ERROR(mlog, "GLFX: error in file <" << filename.toStdString()
                        << ">: "
                        << "No subroutine could be found in program <" <<
                        currentProgram.first.toStdString() << ">.");
        return;
    }

    if (numUniformLocations != count) {
        LOG4CPLUS_ERROR(mlog, "GLFX: error in file <" << filename.toStdString()
                        << ">: "
                        << "User given count (" << count << ") does not equal "
                        << "the number of subroutine uniforms ("
                        << numUniformLocations << ") in program <"
                        << currentProgram.first.toStdString() << ">.");
        return;
    }

    const GLint numActiveSubs = getCurrentProgramSubroutineParam(
                GLActiveSubroutines, shadertype);

    for (int i = 0; i < count; ++i)
    {
        if( GLint(indices[i]) >= numActiveSubs )
        {
            LOG4CPLUS_ERROR(mlog, "GLFX: error in file <"
                            << filename.toStdString() << ">: "
                            << "User given index (" << indices[i]
                            << ") is not in range [0;" << (numActiveSubs - 1)
                            << "] wrt the program <"
                            << currentProgram.first.toStdString() << ">.");
            return;
        }
    }

    glUniformSubroutinesuiv(shadertype, count, indices);
}


void MShaderEffect::setUniformSubroutineByName(const GLenum shadertype,
                                               const QList<QString> subroutines)
{
    if (currentProgram.second == -1) { return; }

    // obtain all information about uniforms and indices
    std::vector<SubroutineUniformInfo> info = getUniformSubroutineInfo(shadertype);

    const GLsizei numSubUniforms = info.size();

    std::vector<GLuint> indices(numSubUniforms);

    // go through all subroutine uniforms and look for the corresponding index
    for (const auto& uniform : info)
    {
        bool subroutineFound = false;

        for (const auto& sub : uniform.compatibleSubroutines)
        {
            if (subroutines.contains(sub.name.c_str()))
            {
                subroutineFound = true;
                indices[uniform.index] = sub.index;
                break;
            }
        }

        if (!subroutineFound)
        {
            LOG4CPLUS_ERROR(mlog, "GLFX: warning in file <"
                            << filename.toStdString() << ">: "
                            << "No given subroutine name was found in "
                            << "subroutine uniform <" << uniform.name
                            << "> in program <"
                            << currentProgram.first.toStdString() << ">.");
            indices[uniform.index] = uniform.compatibleSubroutines[0].index;
        }
    }

    setUniformSubroutine(shadertype, numSubUniforms, &indices[0]);
}


/*******************************************************************************
 ***                           SET UNIFORM UTILS
 *******************************************************************************/

std::shared_ptr<MShaderEffect::Uniform>& MShaderEffect::getUniform(
        const QString name)
{

    QHash<QString, std::shared_ptr<Uniform> >& uniforms
            = activeUniforms[currentProgram.first];

    if (uniforms.find(name) == uniforms.end())
    {
        // search for already detected inactive uniforms
        for (auto& inactive : inactiveUniforms)
        {
            if (inactive->getName() == name)
            {
                return inactive; // return empty shared_ptr
            }
        }

        LOG4CPLUS_ERROR(mlog, "GLFX:  warning in file <"
                        << filename.toStdString() << ">: "
                        << "uniform variable <" << name.toStdString()
                        << "> cannot be found in program <"
                        << currentProgram.first.toStdString()
                        << ">. It is either not in active usage or does not exist.");

        std::shared_ptr<Uniform> uniform
                = std::shared_ptr<Uniform>(new Uniform(-1, -1, 0, 0, name));

        inactiveUniforms.push_back(uniform);

        return inactiveUniforms.at(inactiveUniforms.size() - 1);
    }

    return uniforms[name];
}


}
