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
#include "gxfw/msystemcontrol.h"
#include "util/mutil.h"


namespace GL
{

/*******************************************************************************
***                         CONSTRUCTOR / DESTRUCTOR
*******************************************************************************/

MShaderEffect::MShaderEffect() : numPrograms(0)
{
    programs.clear();
    effect = glfxGenEffect();
}


MShaderEffect::~MShaderEffect()
{
    LOG4CPLUS_DEBUG(mlog, "\t\t -> deleting shader effect "
                    << filename.toStdString());
    release();

    glfxDeleteEffect(effect);
}


/*******************************************************************************
***                            COMPILE METHODS
*******************************************************************************/

void MShaderEffect::release()
{
    for (auto it = programs.begin(); it != programs.end(); ++it)
    {
        GLint prog = it.value();

        if (prog != 0)
        {
            glDeleteProgram(prog);
        }
    }
}


bool MShaderEffect::compileFromFile(const QString _filename)
{
    release();

    numPrograms = 0;
    programs.clear();
    filename = _filename;

    LOG4CPLUS_DEBUG(mlog, "GLFX: compile effect file <"
                    << filename.toStdString() << ">...");

    if (!glfxParseEffectFromFile(effect, _filename.toStdString().c_str()))
    {
        LOG4CPLUS_ERROR(mlog, "GLFX: error in file <"
                        << filename.toStdString() << ">: \n"
                        << glfxGetEffectLog(effect));
        return false;
    }

    if (!compileAllPrograms())
    {
        return false;
    }

    LOG4CPLUS_DEBUG(mlog, "\t-> GLFX: compile process successful!");

    return true;
}


bool MShaderEffect::compileFromFile(const char* _filename)
{
    return compileFromFile(QString(_filename));
}


bool MShaderEffect::compileFromFile_Met3DHome(const QString _filename)
{
    Met3D::MSystemManagerAndControl *sysMC =
            Met3D::MSystemManagerAndControl::getInstance();
    return compileFromFile(sysMC->getMet3DHomeDir().absoluteFilePath(_filename));
}


bool MShaderEffect::compileFromMemory(const char* src)
{
    // release all allocated memory
    release();

    numPrograms = 0;
    // clear all previously created program objects
    programs.clear();

    LOG4CPLUS_DEBUG(mlog, "GLFX: compile effect from memory...");

    if (!glfxParseEffectFromMemory(effect, src))
    {
        LOG4CPLUS_ERROR(mlog, "GLFX: error in file <"
                        << filename.toStdString() << ">: \n"
                        << glfxGetEffectLog(effect));
        return false;
    }

    if (!compileAllPrograms())
    {
        return false;
    }

    LOG4CPLUS_DEBUG(mlog, "\t-> GLFX: compile process successful!");
    return true;
}


bool MShaderEffect::compileAllPrograms()
{
    // get the number of programs contained in the glfx effect file
    numPrograms = glfxGetProgramCount(effect);

    if (numPrograms == 0)
    {
        LOG4CPLUS_ERROR(mlog, "GLFX: error in file <" << filename.toStdString()
                        << ">: no existing programs." << std::flush);
        return false;
    }
    else
    {
        // reserve buckets for all programs
        programs.reserve(numPrograms);

        // loop through all programs, compile them and retrieve required information
        for (int i = 0; i < numPrograms; ++i)
        {
            // get the current program's name
            const char* progName = glfxGetProgramName(effect, i);
            // compile the program
            GLint progObject = glfxCompileProgram(effect, progName);

            QString progNameStr(progName);

            // if one program could not be compiled then return the creation process
            if (progObject < 0)
            {
                LOG4CPLUS_ERROR(mlog, "GLFX: error in file <"
                                << filename.toStdString() << ">: program <"
                                << progName << "> could not be compiled");
                LOG4CPLUS_ERROR(mlog, "GLFX: error in file <"
                                << filename.toStdString() << ">: \n"
                                << glfxGetEffectLog(effect) << std::flush);

                continue;
            }

            programs.insert(progNameStr, progObject);

            GLint numberUniforms = getProgramParam(
                        progNameStr.toStdString().c_str(), GLActiveUniforms);

            QHash<QString, std::shared_ptr<Uniform> > uniformInfos;

            char uniformName[100];
            GLsizei len; // number of chars written to name
            GLint size; // size of uniform var
            GLenum type; // type of uniform var
            GLuint index; // index of uniform var

            for (GLint j = 0; j < numberUniforms; ++j)
            {
                // obtain the location of the specific uniform
                glGetActiveUniform(progObject, j, 100, &len, &size, &type,
                                   uniformName);
                GLint location = glGetUniformLocation(progObject, uniformName);

                const GLchar* uniforms[] = { uniformName, };
                // as index != location
                glGetUniformIndices(progObject, 1, uniforms, &index);

                std::string uniformNameStr(uniformName);
                // test if uniform name defines a certain struct element
                // if not you can cut-off the [..] part of the name
                if (uniformNameStr.find('.') == std::string::npos)
                {
                    uniformNameStr = uniformNameStr.substr(
                                0, uniformNameStr.find('['));
                }

                // save the uniform infos in a hash map of uniforms
                std::shared_ptr<Uniform> uniform = std::shared_ptr<Uniform>(
                            new Uniform(location, index, type, size,
                                        uniformNameStr.c_str()));

                uniformInfos.insert(uniformNameStr.c_str(), uniform);
            }

            numActiveUniforms.insert(progNameStr, numberUniforms);
            activeUniforms.insert(progNameStr, uniformInfos);
        }

        return programs.size() > 0;
    }
}


/*******************************************************************************
***                          BIND / UNBIND METHODS
*******************************************************************************/

void MShaderEffect::bindAttributeLocation(const QString name, const int index)
{
    bindAttributeLocation(name.toStdString().c_str(), index);
}


void MShaderEffect::bindAttributeLocation(const char* name, const int index)
{
    for (auto it = programs.begin(); it != programs.end(); ++it)
    {
        glBindAttribLocation(it.value(), index, name);
    }
}


void MShaderEffect::link()
{
    if (programs.empty())
    {
        LOG4CPLUS_ERROR(mlog, "GLFX: error in file <" << filename.toStdString()
                        << ">: no compiled program could be found.");
        return;
    }

    auto it = programs.begin();
    currentProgram = std::pair<QString, GLint>(it.key(), it.value());

    glLinkProgram(currentProgram.second);
}


void MShaderEffect::bind()
{
    if (programs.empty())
    {
        LOG4CPLUS_ERROR(mlog, "GLFX: error in file <" << filename.toStdString()
                            << ">: no compiled program could be found.");

        currentProgram = std::pair<QString, GLint>("", -1);

        return;
    }

    // take the first program that was created
    auto it = programs.begin();
    currentProgram = std::pair<QString, GLint>(it.key(), it.value());
    // obtain the current program object
    if (currentProgram.first < 0) { return; }

    glUseProgram(currentProgram.second);
}


void MShaderEffect::bindProgram(const QString programName)
{
    if (programs.find(programName) == programs.end())
    {
        LOG4CPLUS_ERROR(mlog, "GLFX: error in file <" << filename.toStdString()
                        << ">: program <"
                        << programName.toStdString() << "> does not exist.");

        currentProgram = std::pair<QString, GLint>("", -1);
        return;
    }

    currentProgram = std::pair<QString, GLint>(programName, programs[programName]);

    glUseProgram(currentProgram.second);
}


/*******************************************************************************
***                        SHADER PROGRAM INFO
*******************************************************************************/

GLint MShaderEffect::getProgramParam(const QString name,
                                     const PROG_PARAM param)
{
    GLint result = -1;

    GLint program = getProgramObject(name);

    if (program < 0)
    {
        return result;
    }

    GLenum query = 0;

    switch(param)
    {
    case GLDeleteStat:
        query = GL_DELETE_STATUS;
        break;
    case GLLinkStatus:
        query = GL_LINK_STATUS;
        break;
    case GLValidateStatus:
        query = GL_VALIDATE_STATUS;
        break;
    case GLInfoLogLength:
        query = GL_INFO_LOG_LENGTH;
        break;
    case GLAttachedShaders:
        query = GL_ATTACHED_SHADERS;
        break;
    case GLActiveAttributes:
        query = GL_ACTIVE_ATTRIBUTES;
        break;
    case GLActiveAttributeMaxLen:
        query = GL_ACTIVE_ATTRIBUTE_MAX_LENGTH;
        break;
    case GLActiveUniforms:
        query = GL_ACTIVE_UNIFORMS;
        break;
    case GLActiveUniformMaxLength:
        query = GL_ACTIVE_UNIFORM_MAX_LENGTH;
        break;
    case  GLProgramBinaryLength:
        query = GL_PROGRAM_BINARY_LENGTH;
        break;
    case GLComputeWorkGroupSize:
        query = GL_COMPUTE_WORK_GROUP_SIZE;
        break;
    case GLTransformFeedbackBufferMode:
        query = GL_TRANSFORM_FEEDBACK_BUFFER_MODE;
        break;
    case GLTransformFeedbackVaryings:
        query = GL_TRANSFORM_FEEDBACK_VARYINGS;
        break;
    case GLTransformFeedbackVaryingMaxLength:
        query = GL_TRANSFORM_FEEDBACK_VARYING_MAX_LENGTH;
        break;
    case GLGeometryVerticesOut:
        query = GL_GEOMETRY_VERTICES_OUT;
        break;
    case GLGeometryInputType:
        query = GL_GEOMETRY_INPUT_TYPE;
        break;
    case GLGeometryOutputType:
        query = GL_GEOMETRY_OUTPUT_TYPE;
        break;
    }

    glGetProgramiv(program, query, &result); CHECK_GL_ERROR;

    return result;
}


GLint MShaderEffect::getProgramSubroutineParam(const QString name,
                                const STAGE_PARAM param,
                                const GLenum shadertype)
{
    GLint result = -1;
    GLint program = getProgramObject(name);

    if (program < 0)
    {
        return result;
    }

    GLenum query = 0;

    switch(param)
    {
    case GLActiveSubroutineUniforms:
        query = GL_ACTIVE_SUBROUTINE_UNIFORMS;
        break;
    case GLActiveSubroutineUniformLocations:
        query = GL_ACTIVE_SUBROUTINE_UNIFORM_LOCATIONS;
        break;
    case GLActiveSubroutines:
        query = GL_ACTIVE_SUBROUTINES;
        break;
    case GLActiveSubroutineUniformMaxLength:
        query = GL_ACTIVE_SUBROUTINE_UNIFORM_MAX_LENGTH;
        break;
    case GLActiveSubroutineMaxLength:
        query = GL_ACTIVE_SUBROUTINE_MAX_LENGTH;
        break;
    }

    glGetProgramStageiv(program, shadertype, query, &result);

    return result;
}


GLint MShaderEffect::getCurrentProgramSubroutineParam(const STAGE_PARAM param,
                                                      const GLenum shadertype)
{
    if (currentProgram.second == -1) { return -1; }

    return getProgramSubroutineParam(currentProgram.first.toStdString().c_str(),
                                     param, shadertype);
}


GLint MShaderEffect::getCurrentProgramParam(const PROG_PARAM param)
{
    if (currentProgram.second == -1) { return -1; }

    return getProgramParam(currentProgram.first.toStdString().c_str(),
                           param);
}


GLint MShaderEffect::getCurrentProgramObject()
{
    return currentProgram.second;
}


GLint MShaderEffect::getProgramObject(const QString name)
{
    if (programs.find(name) == programs.end())
    {
        LOG4CPLUS_ERROR(mlog, "GLFX: error in file <" << filename.toStdString()
                        << ">: program <"
                        << name.toStdString() << "> does not exist.");
        return -1;
    }

    GLint progObj = programs[name];
    return progObj;
}


GLuint MShaderEffect::getSubroutineIndex(const QString name,
                                         const GLenum shadertype)
{
    if (currentProgram.second == -1) { return 0; }

    GLint result = glGetSubroutineIndex(currentProgram.second, shadertype,
                                        name.toStdString().c_str());

    if (result < 0)
    {
        LOG4CPLUS_ERROR(mlog, "GLFX: error in file <" << filename.toStdString()
                        << ">: subroutine <" << name.toStdString()
                        << "> does not exist in program <"
                        << currentProgram.first.toStdString() << ">.");

        return 0;
    }

    return result;
}


GLuint MShaderEffect::getUniformSubroutineIndex(const QString name,
                                                const GLenum shadertype)
{
    if (programs.empty()) { return 0; }

    GLint result =  glGetSubroutineUniformLocation(
                currentProgram.second, shadertype, name.toStdString().c_str());

    if (result < 0)
    {
        LOG4CPLUS_ERROR(mlog, "GLFX: error in file <" << filename.toStdString()
                        << ">: subroutine uniform <" << name.toStdString()
                        << "> does not exist in program <"
                        << currentProgram.first.toStdString() << ">.");

        return 0;
    }

    return result;
}


std::vector<MShaderEffect::SubroutineUniformInfo>
MShaderEffect::getUniformSubroutineInfo(const GLenum shadertype)
{
    GLint numSubUniforms = 0;
    GLint len = 0;
    GLint program = currentProgram.second;

    // query how many active uniforms exist within the current shader program
    numSubUniforms = getCurrentProgramSubroutineParam(
                GLActiveSubroutineUniformLocations, shadertype);

    std::vector<SubroutineUniformInfo> uniforms(numSubUniforms);

    for (int i = 0; i < numSubUniforms; ++i)
    {
        char uniformName[256];
        // get the name of the current uniform
        glGetActiveSubroutineUniformName(program, shadertype,
                                         i, 256, &len, uniformName);

        // fill the uniform info struct
        SubroutineUniformInfo info;
        info.index = glGetSubroutineUniformLocation(program, shadertype,
                                                    uniformName);
        info.name = uniformName;

        GLint numCompSubs = 0;

        // query how many compatible subroutines are compatible with the uniform
        glGetActiveSubroutineUniformiv(program, shadertype, i,
                                       GL_NUM_COMPATIBLE_SUBROUTINES,
                                       &numCompSubs);

        std::vector<GLint> subs(numCompSubs);
        // and get their indices
        glGetActiveSubroutineUniformiv(program, shadertype, i,
                                       GL_COMPATIBLE_SUBROUTINES, &subs[0]);

        info.compatibleSubroutines.resize(numCompSubs);

        for (int j = 0; j < numCompSubs; j++)
        {
            char subName[256];

            glGetActiveSubroutineName(program, shadertype, subs[j],
                                      256, &len, subName);
            // fill the subroutine info
            info.compatibleSubroutines[j].index = glGetSubroutineIndex(
                        program, shadertype, subName);
            info.compatibleSubroutines[j].name = subName;
        }

        uniforms[i] = info;
    }

    return uniforms;
}


void MShaderEffect::printSubroutineInformation(const GLenum shadertype)
{
   std::vector<MShaderEffect::SubroutineUniformInfo> uniformInfos =
           getUniformSubroutineInfo(shadertype);


   for (const auto& uniformInfo : uniformInfos)
   {
        LOG4CPLUS_DEBUG(mlog, "Subroutine Uniform: " << uniformInfo.name
                        << " @location " << uniformInfo.index);

        LOG4CPLUS_DEBUG(mlog, "\t Compatible subroutines:");

        for (const auto& subroutine : uniformInfo.compatibleSubroutines)
        {
            LOG4CPLUS_DEBUG(mlog, "\t -> " << subroutine.name
                            << " @index " << subroutine.index);
        }
   }
}

} // namespace GL
