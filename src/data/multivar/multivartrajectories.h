/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2020-2021 Christoph Neuhauser
**  Copyright 2020      Michael Kern
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
#ifndef MET_3D_MULTIVARTRAJECTORIES_H
#define MET_3D_MULTIVARTRAJECTORIES_H

// standard library imports

// related third party imports
#include "GL/glew.h"
#include <QVector2D>
#ifdef USE_QOPENGLWIDGET
#include <QOpenGLWidget>
#else
#include <QGLWidget>
#endif

// local application imports
#include "../trajectories.h"
#include "gxfw/gl/indexbuffer.h"
#include "gxfw/gl/shaderstoragebufferobject.h"

namespace Met3D {

enum class TrajectorySyncMode {
    TIMESTEP, TIME_OF_ASCENT, HEIGHT
};

struct MFilteredTrajectory
{
    QVector<QVector3D> positions;
    QVector<QVector<float>> attributes;
};
typedef QVector<MFilteredTrajectory> MFilteredTrajectories;

/**
 * Data for one single trajectory for @see MMultiVarTrajectories.
 */
class MMultiVarTrajectory {
public:
    unsigned int getMemorySize_kb() const;

    // Describes the position of variables in the buffer and the total number of variable values for the entire line
    struct LineDesc
    {
        float startIndex; // pointer to index in array
        float numValues; // number of variables along line
    };

    // Describes the range of values for each variable and the offset within each line
    struct VarDesc
    {
        float startIndex;
        QVector2D minMax;
        bool sensitivity;
        QVector<QVector2D> minMaxSens;
    };

    // Point data.
    QVector<QVector3D> positions;
    int lineID;
    QVector<int> elementIDs;

    // Packed array of base trajectory attributes.
    QVector<float> multiVarData;

    // Information about this line/trajectory.
    LineDesc lineDesc;
    // Information about all variables.
    QVector<VarDesc> multiVarDescs;
};

struct MMultiVarTrajectoriesRenderData
{
    bool useGeometryShader = false;
    // IBO
    GL::MIndexBuffer* indexBuffer = nullptr;
    // VBOs (for geometry shader).
    GL::MVertexBuffer* vertexPositionBuffer = nullptr;
    GL::MVertexBuffer* vertexNormalBuffer = nullptr;
    GL::MVertexBuffer* vertexTangentBuffer = nullptr;
    GL::MVertexBuffer* vertexLineIDBuffer = nullptr;
    GL::MVertexBuffer* vertexElementIDBuffer = nullptr;
    // SSBOs (for programmable pull shader).
    GL::MShaderStorageBufferObject* linePointDataBuffer = nullptr;
    // SSBOs
    GL::MShaderStorageBufferObject* variableArrayBuffer = nullptr;
    GL::MShaderStorageBufferObject* lineDescArrayBuffer = nullptr;
    GL::MShaderStorageBufferObject* varDescArrayBuffer = nullptr;
    GL::MShaderStorageBufferObject* lineVarDescArrayBuffer = nullptr;
    GL::MShaderStorageBufferObject* varSelectedArrayBuffer = nullptr;
    GL::MShaderStorageBufferObject* varSelectedTargetVariableAndSensitivityArrayBuffer = nullptr;
    GL::MShaderStorageBufferObject* varDivergingArrayBuffer = nullptr;
    // Region of interest (ROI) selection.
    GL::MShaderStorageBufferObject* roiSelectionBuffer = nullptr;
    /*
     * For horizon graph diagram.
     */
    GL::MShaderStorageBufferObject* lineSelectedArrayBuffer = nullptr;
    GL::MShaderStorageBufferObject* varOutputParameterIdxBuffer = nullptr;
};

struct MTimeStepSphereRenderData
{
    int numSpheres = 1;

    // IBO
    GL::MIndexBuffer* indexBuffer = nullptr;
    // VBOs
    GL::MVertexBuffer* vertexPositionBuffer = nullptr;
    GL::MVertexBuffer* vertexNormalBuffer = nullptr;
    // SSBOs
    GL::MShaderStorageBufferObject* spherePositionsBuffer = nullptr;
    GL::MShaderStorageBufferObject* entrancePointsBuffer = nullptr;
    GL::MShaderStorageBufferObject* exitPointsBuffer = nullptr;
    GL::MShaderStorageBufferObject* lineElementIdsBuffer = nullptr;
};

struct MTimeStepRollsRenderData
{
    // IBO
    GL::MIndexBuffer* indexBuffer = nullptr;
    // VBOs
    GL::MVertexBuffer* vertexPositionBuffer = nullptr;
    GL::MVertexBuffer* vertexNormalBuffer = nullptr;
    GL::MVertexBuffer* vertexTangentBuffer = nullptr;
    GL::MVertexBuffer* vertexRollPositionBuffer = nullptr;
    GL::MVertexBuffer* vertexLineIdBuffer = nullptr;
    GL::MVertexBuffer* vertexLinePointIdxBuffer = nullptr;
    GL::MVertexBuffer* vertexVariableIdAndIsCapBuffer = nullptr;
};

struct LineElementIdData {
    float centerIdx;
    float entranceIdx;
    float exitIdx;
    int lineId;
};

struct ROISelection {
    uint32_t roiVarAIndex, roiVarBIndex;
    float roiVarALower, roiVarAUpper;
    float roiVarBLower, roiVarBUpper;
};

/**
 * Flow line data with multiple variables being displayed at once.
 */
class MMultiVarTrajectories : public MSupplementalTrajectoryData
{
public:
    MMultiVarTrajectories(
            MDataRequest requestToReferTo, const MFilteredTrajectories& filteredTrajectories,
            const QVector<int>& trajIndicesToFilteredIndicesMap,
            unsigned int numSens, unsigned int numAux, unsigned int numVariables,
            const QStringList& auxDataVarNames, const QStringList& outputParameterNames,
            bool useGeometryShader, int tubeNumSubdivisions);
    ~MMultiVarTrajectories() override;

    unsigned int getMemorySize_kb() override;

    inline int size() const { return multiVarTrajectories.size(); }
    inline MMultiVarTrajectory& operator[](std::size_t idx) { return multiVarTrajectories[idx]; }
    inline const MMultiVarTrajectory& operator[](std::size_t idx) const { return multiVarTrajectories[idx]; }
    inline const MFilteredTrajectories& getBaseTrajectories() const { return baseTrajectories; }

    // For trajectory filtering.
    inline void setDirty(bool isDirty) { this->isDirty = isDirty; }
    void updateTrajectorySelection(
            const GLint* startIndices, const GLsizei* indexCount,
            int numTimeStepsPerTrajectory, int numFilteredTrajectories);
    bool getUseFiltering();
    inline int getNumTrajectoriesTotal() const { return multiVarTrajectories.size(); }
    int getNumFilteredTrajectories();
    GLsizei* getTrajectorySelectionCount();
    const void* const* getTrajectorySelectionIndices();
    bool getFilteredTrajectories(
            const GLint* startIndices, const GLsizei* indexCount,
            int numTimeStepsPerTrajectory, int numSelectedTrajectories,
            QVector<QVector<QVector3D>>& trajectories,
            QVector<QVector<float>>& trajectoryPointTimeSteps,
            QVector<uint32_t>& _selectedTrajectoryIndices);

    MMultiVarTrajectoriesRenderData getRenderData(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr);
#else
    QGLWidget *currentGLContext = nullptr);
#endif
    void releaseRenderData();
    void updateSelectedVariableIndices(const QVector<uint32_t>& selectedVariableIndices);
    void updateDivergingVariables(const QVector<uint32_t>& varDiverging);
    void updateSelectedLines(const QVector<uint32_t>& selectedLines);
    void updateOutputParameterIdx(const int OutputParameterIdx);
    void updateROI(const ROISelection &roiSelection);

    // Used for aligning warm conveyor belt trajectories based on their ascension.
    void updateLineAscentTimeStepArrayBuffer(
            const QVector<int>& _ascentTimeStepIndices, int _maxAscentTimeStepIndex);

    MTimeStepSphereRenderData* getTimeStepSphereRenderData(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr);
#else
            QGLWidget *currentGLContext = nullptr);
#endif
    bool updateTimeStepSphereRenderDataIfNecessary(
            TrajectorySyncMode trajectorySyncMode, int timeStep, uint32_t syncModeTrajectoryIndex, float sphereRadius,
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr);
#else
            QGLWidget *currentGLContext = nullptr);
#endif
    void releaseTimeStepSphereRenderData();

    inline const QVector<QVector4D>& getSpherePositions() const { return spherePositions; }
    inline const QVector<QVector4D>& getSphereEntrancePoints() const { return entrancePoints; }
    inline const QVector<QVector4D>& getSphereExitPoints() const { return exitPoints; }
    inline const QVector<LineElementIdData>& getSphereLineElementIds() const { return lineElementIds; }

    MTimeStepRollsRenderData* getTimeStepRollsRenderData(
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr);
#else
    QGLWidget *currentGLContext = nullptr);
#endif
    void updateTimeStepRollsRenderDataIfNecessary(
            TrajectorySyncMode trajectorySyncMode, int timeStep, uint32_t syncModeTrajectoryIndex, float tubeRadius,
            float rollsRadius, float rollsWidth, bool mapRollsThickness, int numLineSegments,
#ifdef USE_QOPENGLWIDGET
            QOpenGLWidget *currentGLContext = nullptr);
#else
            QGLWidget *currentGLContext = nullptr);
#endif
    void releaseTimeStepRollsRenderData();

private:
    MFilteredTrajectories baseTrajectories;
    QVector<MMultiVarTrajectory> multiVarTrajectories;
    MMultiVarTrajectoriesRenderData multiVarTrajectoriesRenderData;
    QVector<uint32_t> selectedVariableIndices;
    QVector<uint32_t> varDiverging;
    QVector<uint32_t> trajectoryIndexOffsets;
    QVector<uint32_t> numIndicesPerTrajectory;
    QVector<uint32_t> selectedLines;
    QVector<uint32_t> targetVariableAndSensitivityIndexArray;
    bool useGeometryShader = false;
    int tubeNumSubdivisions = 8; ///< Only for !useGeometryShader.

    // Used for aligning warm conveyor belt trajectories based on their ascension.
    QVector<int> ascentTimeStepIndices;
    int maxAscentTimeStepIndex = 0;

    // Focus spheres data.
    TrajectorySyncMode lastSphereTrajectorySyncMode = TrajectorySyncMode::TIMESTEP;
    int lastSphereTimeStep = std::numeric_limits<int>::lowest();
    uint32_t lastSphereSyncModeTrajectoryIndex = 0;
    float lastSphereRadius = std::numeric_limits<float>::lowest();
    QVector<QVector4D> spherePositions;
    QVector<QVector4D> entrancePoints;
    QVector<QVector4D> exitPoints;
    QVector<LineElementIdData> lineElementIds;
    MTimeStepSphereRenderData timeStepSphereRenderData;
    const QString timeStepSphereIndexBufferID =
            QString("timestepsphere_index_buffer_#%1").arg(getID());
    const QString timeStepSphereVertexPositionBufferID =
            QString("timestepsphere_vertex_position_buffer_#%1").arg(getID());
    const QString timeStepSphereVertexNormalBufferID =
            QString("timestepsphere_vertex_normal_buffer_#%1").arg(getID());
    const QString timeStepSpherePositionsBufferID =
            QString("timestepsphere_sphere_positions_buffer_#%1").arg(getID());
    const QString timeStepSphereEntrancePointsBufferID =
            QString("timestepsphere_sphere_entrance_points_buffer_#%1").arg(getID());
    const QString timeStepSphereExitPointsBufferID =
            QString("timestepsphere_sphere_exit_points_buffer_#%1").arg(getID());
    const QString timeStepSphereLineElementIdsBufferID =
            QString("timestepsphere_line_element_ids_buffer_#%1").arg(getID());

    // Focus rolls data.
    TrajectorySyncMode lastRollsTrajectorySyncMode = TrajectorySyncMode::TIMESTEP;
    int lastRollsTimeStep = std::numeric_limits<int>::lowest();
    uint32_t lastRollsSyncModeTrajectoryIndex = 0;
    float lastTubeRadius = std::numeric_limits<float>::lowest();
    float lastRollsRadius = std::numeric_limits<float>::lowest();
    float lastRollsWidth = std::numeric_limits<float>::lowest();
    bool lastMapRollsThickness = false;
    int lastNumLineSegmentsRolls = 8;
    QVector<uint32_t> lastVarSelectedRolls;
    MTimeStepRollsRenderData timeStepRollsRenderData;
    const QString timeStepRollsIndexBufferID =
            QString("timesteprolls_index_buffer_#%1").arg(getID());
    const QString timeStepRollsVertexPositionBufferID =
            QString("timesteprolls_vertex_position_buffer_#%1").arg(getID());
    const QString timeStepRollsVertexNormalBufferID =
            QString("timesteprolls_vertex_normal_buffer_#%1").arg(getID());
    const QString timeStepRollsVertexTangentBufferID =
            QString("timesteprolls_vertex_tangent_buffer_#%1").arg(getID());
    const QString timeStepRollsPositionBufferID =
            QString("timesteprolls_rolls_position_buffer_#%1").arg(getID());
    const QString timeStepRollsVertexLineIdBufferID =
            QString("timesteprolls_rolls_vertex_line_id_buffer_#%1").arg(getID());
    const QString timeStepRollsVertexLinePointIdxBufferID =
            QString("timesteprolls_rolls_vertex_point_idx_buffer_#%1").arg(getID());
    const QString timeStepRollsVertexVariableIdAndIsCapBufferBufferID =
            QString("timesteprolls_vertex_variable_id_and_is_cap_buffer_#%1").arg(getID());

    // Data for trajectory filtering.
    bool isDirty = true;
    QVector<int> trajIndicesToFilteredIndicesMap;
    int numTrajectories = 0;
    uint32_t numVariables = 0;
    uint32_t numAux = 0;
    uint32_t numTimesteps = 0;
    bool useFiltering = false;
    bool hasFilteringChangedSphere = false;
    bool hasFilteringChangedRolls = false;
    int numFilteredTrajectories = 0;
    GLsizei* trajectorySelectionCount = nullptr;
    ptrdiff_t* trajectorySelectionIndices = nullptr;
    QVector<int> trajectoryCompletelyFilteredMap;

    QVector<QVector2D> minMaxAttributes;

    QVector<uint32_t> OutputParameterIdx;
//    int selectedOutputParameterIdx = 0;

    const QString indexBufferID =
            QString("multivartrajectories_index_buffer_#%1").arg(getID());
    const QString vertexPositionBufferID =
            QString("multivartrajectories_vertex_position_buffer_#%1").arg(getID());
    const QString vertexNormalBufferID =
            QString("multivartrajectories_vertex_normal_buffer_#%1").arg(getID());
    const QString vertexTangentBufferID =
            QString("multivartrajectories_vertex_tangent_buffer_#%1").arg(getID());
    const QString vertexLineIDBufferID =
            QString("multivartrajectories_vertex_multi_variable_buffer_#%1").arg(getID());
    const QString vertexElementIDBufferID =
            QString("multivartrajectories_vertex_variable_desc_buffer_#%1").arg(getID());
    const QString linePointDataBufferID =
            QString("multivartrajectories_line_point_data_buffer_#%1").arg(getID());
    const QString variableArrayBufferID =
            QString("multivartrajectories_variable_array_buffer_#%1").arg(getID());
    const QString lineDescArrayBufferID =
            QString("multivartrajectories_line_desc_array_buffer_#%1").arg(getID());
    const QString varDescArrayBufferID =
            QString("multivartrajectories_var_desc_array_buffer_#%1").arg(getID());
    const QString lineVarDescArrayBufferID =
            QString("multivartrajectories_line_var_desc_array_buffer_#%1").arg(getID());
    const QString varSelectedArrayBufferID =
            QString("multivartrajectories_var_selected_array_buffer_#%1").arg(getID());
    const QString varSelectedTargetVariableAndSensitivityArrayBufferID =
            QString("multivartrajectories_var_selected_target_variable_and_sensitivity_array_buffer_#%1").arg(getID());
    const QString varDivergingArrayBufferID =
            QString("multivartrajectories_var_diverging_array_buffer_#%1").arg(getID());
    const QString roiSelectionBufferID =
            QString("multivartrajectories_roi_selection_buffer_#%1").arg(getID());
    const QString lineSelectedArrayBufferID =
            QString("multivartrajectories_line_selected_array_buffer_#%1").arg(getID());
    const QString varOutputParameterIdxBufferID =
            QString("multivartrajectories_var_outputparameter_buffer_#%1").arg(getID());
};

}

#endif //MET_3D_MULTIVARTRAJECTORIES_H
