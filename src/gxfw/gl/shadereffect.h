/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015-2017 Marc Rautenhaus
**  Copyright 2015      Michael Kern
**  Copyright 2016      Bianca Tost
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
#ifndef SHADEREFFECT_H
#define SHADEREFFECT_H

#include <string>
#include <iostream>
#include <unordered_map>
#include <map>
#include <assert.h>
#include <memory>

#include <QMatrix2x2>
#include <QMatrix3x3>

#include "GL/glfx.h"
#include "GL/glew.h"

//! TODO
//! o simplify / delete some hashmaps?

namespace GL
{

/**
  @brief Abstraction of GLSL shaders. MShaderEffect encapsulates GLSL shader
  programs and provides a simple C++ interface to load/compile programs,
  bind the programs and to set uniforms.

  Wraps the glfx library [https://code.google.com/p/glfx/]. See the glfx
  documentation for information on how the shader sources should be structured.
 */
class MShaderEffect
{
public:
    MShaderEffect();

    virtual ~MShaderEffect();

    /**
      Compile GLSL shader sources from glfx formatted file @p _filename.
     */
    bool compileFromFile(const char* _filename);

    bool compileFromFile(const QString _filename);

    /**
      Compile GLSL shader sources from glfx formatted file located relative
      to Met.3D home directory.

      @see MSystemManagerAndControl::getMet3DHomeDir()
     */
    bool compileFromFile_Met3DHome(const QString _filename);

    bool compileFromMemory(const char* src);

    /**
      Set uniform @p name to value @p x.
     */
    void setUniformValue(const QString name, const GLint x); // INT

    void setUniformValue(const QString name, const GLuint x); // UINT

    void setUniformValue(const QString name, const GLboolean x); // BOOL

    void setUniformValue(const QString name, const GLfloat x); // FLOAT

    void setUniformValue(const QString name, const GLdouble x); // DOUBLE

    /**
      Set uniform of type "<datatype>4 vector".
     */
    void setUniformValue(const QString name, const GLdouble x, // DOUBLE4
                         const GLdouble y,
                         const GLdouble z,
                         const GLdouble w);

    void setUniformValue(const QString name, const GLfloat x, // FLOAT4
                         const GLfloat y,
                         const GLfloat z,
                         const GLfloat w);

    void setUniformValue(const QString name, const QVector4D& vec4);

    void setUniformValue(const QString name, const GLint x,
                         const GLint y,
                         const GLint z,
                         const GLint w); // INT4

    void setUniformValue(const QString name, const GLuint x,
                         const GLuint y,
                         const GLuint z,
                         const GLuint w); // UINT4

    void setUniformValue(const QString name, const GLboolean x,
                         const GLboolean y,
                         const GLboolean z,
                         const GLboolean w); // BOOL4

    void setUniformValue(const QString name, const QColor& color); // VEC4

    /**
      Set uniform of type "<datatype>3 vector".
     */
    void setUniformValue(const QString name, const GLdouble x,
                         const GLdouble y,
                         const GLdouble z); // DOUBLE3

    void setUniformValue(const QString name, const GLfloat x,
                         const GLfloat y,
                         const GLfloat z); // FLOAT3

    void setUniformValue(const QString name, const QVector3D& vec3);

    void setUniformValue(const QString name, const GLint x,
                         const GLint y,
                         const GLint z); // INT3

    void setUniformValue(const QString name, const GLboolean x,
                         const GLboolean y,
                         const GLboolean z); // BOOL3

    void setUniformValue(const QString name, const GLuint x,
                         const GLuint y,
                         const GLuint z); // UINT3

    /**
      Set uniform of type "<datatype>2 vector".
     */
    void setUniformValue(const QString name, const GLdouble x,
                         const GLdouble y); // DOUBLE2

    void setUniformValue(const QString name, const GLfloat x,
                         const GLfloat y); // FLOAT2

    void setUniformValue(const QString name, const QVector2D& vec2);

    void setUniformValue(const QString name, const GLint x,
                         const GLint y); // INT2

    void setUniformValue(const QString name, const GLuint x,
                         const GLuint y); // UINT2

    void setUniformValue(const QString name, const GLboolean x,
                         const GLboolean y); // BOOL2

    void setUniformValue(const QString name, const QPoint& point); // IVEC2

    void setUniformValue(const QString name, const QPointF& point); // VEC2

    /**
      Set uniform of matrix type.
     */
    void setUniformValue(const QString name, const QMatrix4x4& matrix);

    void setUniformValue(const QString name, const QMatrix3x3& matrix);

    void setUniformValue(const QString name, const QMatrix2x2& matrix);

    void setUniformValue(const QString name, const QMatrix2x3& matrix);

    void setUniformValue(const QString name, const QMatrix2x4& matrix);

    void setUniformValue(const QString name, const QMatrix3x2& matrix);

    void setUniformValue(const QString name, const QMatrix3x4& matrix);

    void setUniformValue(const QString name, const QMatrix4x2& matrix);

    void setUniformValue(const QString name, const QMatrix4x3& matrix);

    /**
      Set uniform array.
     */
    void setUniformValueArray(const QString name, const GLboolean* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const GLboolean* data,
                              const int8_t count, const int8_t tupleSize);

    void setUniformValueArray(const QString name, const GLint* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const GLint* data,
                              const int8_t count, const int8_t tupleSize);

    void setUniformValueArray(const QString name, const GLuint* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const GLuint* data,
                              const int8_t count, const int8_t tupleSize);

    void setUniformValueArray(const QString name, const GLfloat* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const GLfloat* data,
                              const int8_t count, const int8_t tupleSize);

    void setUniformValueArray(const QString name, const GLdouble* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const GLdouble* data,
                              const int8_t count, const int8_t tupleSize);

    void setUniformValueArray(const QString name, const QMatrix4x4* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const QMatrix3x3* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const QMatrix2x2* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const QMatrix2x3* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const QMatrix2x4* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const QMatrix3x2* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const QMatrix3x4* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const QMatrix4x2* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const QMatrix4x3* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const QVector2D* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const QVector3D* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const QVector4D* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const QColor* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const QPoint* data,
                              const int8_t count);

    void setUniformValueArray(const QString name, const QPointF* data,
                              const int8_t count);
    void setUniformValueArray(const QString name, const GLuint64* data,
                              const int8_t count);

    /**
      Set uniform subroutine.
     */
    void setUniformSubroutine(const GLenum shadertype,
                              const std::vector<GLuint>& indices);

    void setUniformSubroutine(const GLenum shadertype,
                              const GLsizei count, const GLuint* indices);

    void setUniformSubroutineByName(const GLenum shadertype,
                                    const QList<QString> subroutines);

    bool compileAllPrograms();

    void bindAttributeLocation(const QString name, const int index);

    void bindAttributeLocation(const char* name, const int index);

    void link();

    /**
      Bind the default program defined in the glfx file.
     */
    void bind();

    /**
      Bind a specific program defined in the glfx file.
     */
    void bindProgram(const QString programName);

    enum PROG_PARAM
    {
        GLDeleteStat = 0, // program currently flagged for deletion?
        GLLinkStatus, // link of program was successful?
        GLValidateStatus, // program validation is successful?
        GLInfoLogLength, // num of chars in info log for program
        GLAttachedShaders, // number of attached shader objects
        GLActiveAttributes, // number of active attributes
        GLActiveAttributeMaxLen, // length of longest active attribute name
        GLActiveUniforms, // number of active uniform values
        GLActiveUniformMaxLength, // max length of name of active attributes
        GLProgramBinaryLength, // length of program binary
        GLComputeWorkGroupSize, // array of three ints of local workgroup size
        GLTransformFeedbackBufferMode, // symbolic constant -> in feedback mode
        GLTransformFeedbackVaryings, // number of varying constants
        GLTransformFeedbackVaryingMaxLength, // longest variable name
        GLGeometryVerticesOut, // maximum number of vertices
        GLGeometryInputType, // primitive input type
        GLGeometryOutputType // primitive output type
    };

    enum STAGE_PARAM
    {
        GLActiveSubroutineUniforms = 0, // number of active subroutine variables
        GLActiveSubroutineUniformLocations, // number of active subroutine variable locations
        GLActiveSubroutines, // number of active subroutines
        GLActiveSubroutineUniformMaxLength, // length of longest subroutine uniform
        GLActiveSubroutineMaxLength // length of the longest subroutine name
    };

    GLint getProgramParam(const QString name, const PROG_PARAM param);

    GLint getCurrentProgramParam(const PROG_PARAM param);

    GLint getCurrentProgramObject();

    GLint getProgramObject(const QString name);

    /**
      Obtain the index of the uniform subroutine indexed in an array of
      subroutines.
     */
    GLuint getUniformSubroutineIndex(const QString name,
                                     const GLenum shadertype);

    /**
      Obtain the index of the subroutine w.r.t. a uniform subroutine.
     */
    GLuint getSubroutineIndex(const QString name,
                              const GLenum shadertype);

    GLint getProgramSubroutineParam(const QString name,
                                    const STAGE_PARAM param,
                                    const GLenum shadertype);

    GLint getCurrentProgramSubroutineParam(const STAGE_PARAM param,
                                           const GLenum shadertype);

    struct SubroutineInfo
    {
        std::string name;
        GLuint index;
    };

    struct SubroutineUniformInfo
    {
        std::string name;
        GLuint index;
        std::vector<SubroutineInfo> compatibleSubroutines;
    };

    void printSubroutineInformation(const GLenum shadertype);

protected:

    // "usual" uniform object containing the necessary attributes
    class Uniform
    {
    public:
        Uniform(const GLint _location, const GLuint _index,
                const GLenum _type, const GLuint _size,
                const QString _name);

        virtual ~Uniform();

        bool setUniform(const GLenum inputType, const void* data,
                        const GLuint count, const GLuint inputTupleSize);

        QString getName() const;
        GLint getLocation() const;
        GLuint getIndex() const;
        GLuint getSize() const;
        GLenum getType() const;

    private:

        GLint location;
        GLuint index;
        GLenum type;
        GLuint size;
        QString name;

        const char* glEnumToSring(GLenum type) const;
    };


    // current glfx effect
    GLint  effect;

    // all compiled programs
    QHash<QString, GLint> programs;
    // number of available programs
    int8_t  numPrograms;
    // current active bound program
    std::pair<QString, GLint>  currentProgram;
    // number of active uniforms of one program
    QHash<QString, GLint> numActiveUniforms;

    // inactive or non-existent uniforms that were used by program
    std::vector< std::shared_ptr<Uniform> > inactiveUniforms;

    // active uniforms and their information within a program
    QHash<QString, QHash< QString, std::shared_ptr<Uniform> > > activeUniforms;

    // filename of compiled glsl file
    QString filename;

    void release();

    // Internal uniform functions
    // ==========================

    // retrieve information about a certain uniform with given name
    std::shared_ptr<Uniform>& getUniform(const QString name);

    // collects information of all subroutine uniforms and compatible subroutines
    std::vector<SubroutineUniformInfo> getUniformSubroutineInfo(const GLenum shadertype);


    void setUniformMatrixXY(const QString name, const int8_t count,
                            const int8_t cols, const int8_t rows,
                            const GLenum type, const qreal* data);

    void setUniformMatrixXY(const QString name, const int8_t count,
                            const int8_t cols, const int8_t rows,
                            const GLenum type, const float* data);

};

}

#endif // MSHADEREFFECT_H
