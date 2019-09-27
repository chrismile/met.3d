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
*******************************************************************************/
#ifndef PLOTCOLLECTION_H
#define PLOTCOLLECTION_H

// standard library imports

// related third party imports
#include <GL/glew.h>
#include <QtCore>
#include <QtProperty>

// local application imports
#include "gxfw/msceneviewglwidget.h"
#include "util/mstopwatch.h"

#define MSTOPWATCH_ENABLED

namespace Met3D
{

class MNWPActorVariable;
class MNWP2DSectionActorVariable;
class MNWPHorizontalSectionActor;

/**
  @brief MPlotCollection represents a collection of different types of plots,
  in particular: contour boxplots, contour probability plots, spaghetti plots
  and distance and scalar variability plots. For now these plots are only
  implemented for @ref MNWPHorizontalSectionActor.
 */
class MPlotCollection : public QObject
{
    Q_OBJECT

public:
    /**
      MPlotCollection sets pointers to the shader effects used for computing and
      rendering the spaghetti, contour and variability plots and adds entries to
      the render mode property of @p var.

      It initializes QtPropertyBrowser properties needed to customize spaghetti,
      contour and variability plots. The properties are displayed as subgroups
      of the parent variable @p var.
     */
    MPlotCollection(MNWP2DSectionActorVariable *var);

    ~MPlotCollection();

    bool onQtPropertyChanged(QtProperty *property);

    void saveConfiguration(QSettings *settings);

    void loadConfiguration(QSettings *settings);

    void reset();

    /** Resets recompute-indicator-variables of all plots of true. */
    void needsRecomputation();

    /**
      Deletes @ref gridDataStorage if necessary and creates a new array.

      Called by @ref MNWPHorizontalSectionActor::renderGridVerticalInterpolation() .
     */
    void recreateArrays();

    /**
      Calls computation methods to generate informations (render textures, etc.)
      necessary to render spaghetti, contour and variability plots.

      compute() only computes the informations for a plot if it is selected and
      if the information needs to be [re]computed.

      Spaghetti plots do not have any computation methods at the moment.

      This method is called by
      @ref MNWPHorizontalSectionActor::renderToCurrentContext().
     */
    void compute();

    /**
      Calls render methods to render spaghetti, contour and variability plots.

      render() renders a plot only if the plotting technique is selected.

      This method is called by
      @ref MNWPHorizontalSectionActor::renderToCurrentContext().
     */
    void render(MSceneViewGLWidget *sceneView);

    /**
      Sets @ref glMarchingSquaresShader, @ref glContourPlotsShader,
      @ref glCSContourPlotsShader and @ref glVariabilityPlotsShader to pointers
      to shaders used for computing and rendering spaghetti plots, contour plots
      and variability plots.

      It accesses the shader pointers of @ref actor to assign them to their
      according variables.

      This method is called i.a. by
      @ref MNWPHorizontalSectionActor::initializeActorResources().
     */
    void setShaders();

    /**
      Updates @ref contourSetProperty if a contour set was added to or deleted
      from @ref var and adapts @ref contourSetIndex if necessary.
      
      If the currently selected contour set was deleted, @ref contourSetIndex is
      set to 0.
      
      @param index holds the index of the contour set deleted or -1 if a
      contour set was added.
     */
    void updateContourList(int index=-1);

    // Array storing the CPU version of the scalar fields of all selected
    // members.
    float *gridDataStorage;

    // Texture storing values for drawing contours, used by contour probability
    // plot and variability plots to upload corresponding data and to draw
    // median respectively mean contours. It contains one channel.
    GL::MTexture *textureLineDrawing;

/*------------------------------ Spaghetti Plot ------------------------------*/
    struct SpaghettiPlot
    {
        SpaghettiPlot(MActor *actor, QtProperty *renderGroup);

        void saveConfiguration(QSettings *settings);
        void loadConfiguration(QSettings *settings, MQtProperties *properties);

        QtProperty *groupProperty;
        QtProperty *colourProperty;
        QtProperty *thicknessProperty;
        QtProperty *prismaticColouredProperty;

        QColor      colour;             // colour of spaghetti plot
        double      thickness;          // thickness of spaghetti plot
        bool        prismaticColoured;  // use different colours for each contour
    };

    SpaghettiPlot *spaghettiPlot;

/*------------------------------ Contour Boxplot -----------------------------*/
/*------------------------- Contour Probability Plot -------------------------*/
    struct ContourPlot
    {
        ContourPlot(MActor *actor, QtProperty *renderGroup);

        void saveConfiguration(QSettings *settings);
        void loadConfiguration(QSettings *settings, MQtProperties *properties);

        // Group properties.
        QtProperty *groupProperty;
        QtProperty *contourSetProperty;
        QtProperty *innerColourProperty;
        QtProperty *drawOuterProperty;
        QtProperty *outerColourProperty;
        // properties for contour boxplot only
        QtProperty *BoxplotGroupProperty;
        QtProperty *drawMedianProperty;
        QtProperty *medianThicknessProperty;
        QtProperty *medianColourProperty;
        QtProperty *drawOutliersProperty;
        QtProperty *outlierThicknessProperty;
        QtProperty *outlierColourProperty;
        QtProperty *epsilonProperty;
        QtProperty *useDefaultEpsilonProperty;
        // properties for contour probability plot only
        QtProperty *probabilityPlotGroupProperty;
        QtProperty *innerPercentageProperty;
        QtProperty *outerPercentageProperty;
        QtProperty *drawoutermostProperty;
        QtProperty *outermostColourProperty;

        // Variables.
        // Variables used by both contour plot types.
        QColor innerColour;    // colour of innermost band
        bool   drawOuter;      // indicator variable for drawing outer band
        QColor outerColour;    // colour of outer band
        bool   drawMedian;     // indicator for drawing the median line
        double medianThickness;// thickness of median line
        QColor medianColour;   // colour of median line
        // Variables used by contour boxplot only.
        bool   boxplotNeedsRecompute; // indicator for complete recomputation of
                                      // cbp (binary map, default epsilon, plot)
        bool   drawOutliers;     // indicator for drawing outliers
        double outlierThickness; // thickness of outliers
        QColor outlierColour;    // colour of outliers
        double epsilon; // tolerance threshold for deciding whether a contour
                        // lies in between two others contours or not.
                        // 0.0="no failures", 1.0="all grid points may fail"
        double defaultEpsilon; // default epsilon computed by some metrics
        bool   useDefaultEpsilon; // indicator for using the default epsilon
        bool   epsilonChanged; // indicator for changed epsilon
                               // -> only contour boxplot needs recomputation
                               // but neither binary maps nor default epsilon
        // Variables used by contour probability plot only.
        bool   probabilityNeedsRecompute; // indicator for recomputing contour
                                          // probability plot
        double innerPercentage; // "width" of inner band in percentage of
                                // ensemble members
        double outerPercentage; // "width" of one half of the outermost
                                // band in percentage of ensemble members
        bool   drawOutermost;   // indicator for drawing the outermost band
        QColor outermostColour; // colour of outermost band

        // arrays
        float *bandDepth; // stores the band depth values and related
                          // member indices: member*10+bandDepth
                          // with "member" corresponding to the member's index
                          // in "grids" list of var->gridAggregation.
        float *probabilityMedian; // array storing the median value of a contour
                                  // probability plot for each grid point
    };

    ContourPlot *contourPlot;

    // Texture storing values for drawing the contour boxplot,
    // 4 channels: (innerMin, innerMax, outerMin, outerMax)
    GL::MTexture *textureContourBoxplot;
    // Texture storing band depth array with band depth values and related
    // member indices: ((memberIndex * 10) + bandDepth)
    GL::MTexture *textureCBPBandDepth;
    // Texture storing matrix used to compute a default epsilon.
    GL::MTexture *textureCBPEpsilonMatrix;
    // Texture storing values for drawing the contour probability plot,
    // 4 channels, 2*gridSize: ((innerMin,outmostMin), (innerMax,outmostMax),
    // outerMin, outerMax).
    GL::MTexture *textureContourProbabilityPlot;

    // Container holding textures for storing contour boxplot binary maps.
    QVector<GL::MTexture*> binaryMapTextureContainer;

/*------------------------- Distance Variability Plot ------------------------*/
/*-------------------------- Scalar Variability Plot -------------------------*/
    struct VariabilityPlot
    {
        VariabilityPlot(MActor *actor, QtProperty *renderGroup);

        void saveConfiguration(QSettings *settings);
        void loadConfiguration(QSettings *settings, MQtProperties *properties);

        QtProperty *groupProperty;
        QtProperty *colourProperty;
        QtProperty *scaleProperty;
        QtProperty *drawMeanProperty;
        QtProperty *meanThicknessProperty;
        QtProperty *meanColourProperty;

        QColor colour;        // colour of variability plot band
        double scale;         // scale of standard deviation/band width
        bool   drawMean;      // indicator for drawing mean line
        double meanThickness; // thickness of mean line
        QColor meanColour;    // colour of mean line

        // Distance variability plot variables.
        bool distanceNeedsRecompute; // indicator for recomputing whole distance
                                     // variability plot (distance field, plot)
        bool distanceScaleChanged;   // indicator for recomputing only render
                                     // texture as scale changed (distance
                                     // fields stay the same)
        QVector<float*> distanceStorage; // vector storing distance fields 
                                         // (one per member and iso value)
        QVector<float*> distanceMean; // vector storing mean distance fields
                                      // (one per iso value)

        // Scalar variability plot variables.
        bool   scalarNeedsRecompute; // indicator for recomputing scalar
                                     // variability plot
        float *scalarMean; // array storing mean scalar value for each grid point

    };

    // Container holding textures for distance variability plot (one per iso
    // value); 2 channels: (min, max).
    QVector<GL::MTexture*> distanceTextureContainer;

    VariabilityPlot *variabilityPlot;

    // Texture storing values for drawing the scalar variability plot.
    // 2 channels: (min, max)
    GL::MTexture *textureScalarVariabilityPlot;

protected:
    friend class MNWPActorVariable;
    friend class MNWP2DSectionActorVariable;
    friend class MNWP2DHorizontalActorVariable;

    /** Variable that this instance belongs to. */
    MNWP2DSectionActorVariable *var;

    /** Actor that this instance belongs to. */
    MNWPHorizontalSectionActor *actor;

private:
    /** Deletes the given texture if necessary and sets its pointer to nullptr. */
    void textureDelete(GL::MTexture *texture);

    /**
      Computes index of a texture storing results belonging to @p isovalue and
      @p member.

      Can be used to get the index of a texture only related to an ensemble
      member as well by setting @p isovalue and @p numMembers to zero.
     */
    inline int getTextureIndex(int isovalue, int numMembers, int member);

    /** Creates textures needed by different plotting techniques. */
    void createGeneralTextures();

    /** Creates textures needed by contour boxplots. */
    void createContourBoxplotTextures();
    void createBinaryMapTextures(int amountOfTexturesNeeded);

    /** Creates textures needed by contour probability plot. */
    void createContourProbabilityPlotTextures();

    /** Creates textures needed by variability plots. */
    void createScalarVariabilityPlotTexturesAndArrays();
    void createDistanceVariabilityPlotTexturesAndArrays();

    // Methods handling bindless textures.
    /**
      "Activates" textures of ensemble members for usage as bindless textures.
      Textures need to be "released" with @ref nonResidentBindlessMembers()
      after computation/rendering is done.
     */
    void residentBindlessMembers(std::shared_ptr<GL::MShaderEffect> shader);
    /**
      Releases bindless textures of ensemble members by making them non
      resident. Needs to be called after @ref residentBindlessTextures() when
      bindless textures are no longer needed.
     */
    void nonResidentBindlessMembers();

    /**
      "Activates" textures in @p textureContainer for usage as bindless
      textures. Textures need to be "released" by calling
      @ref nonResidentBindlessTextures() after computation/rendering is done.
     */
    void residentBindlessTextures(std::shared_ptr<GL::MShaderEffect> shader,
                                  QVector<GL::MTexture*> *textureContainer);
    /**
      Releases bindless textures by making them non resident. Needs to be
      called after @ref residentBindlessTextures() when bindless textures are
      no longer needed.
     */
    void nonResidentBindlessTextures(QVector<GL::MTexture*> *textureContainer);

/*----------------------------- Compute Methods ------------------------------*/
    // Computation methods for contour boxplots.
    /**
      Compute binary map for a given iso value (first iso value of selected
      contour set) per selected ensemble member.

      The binary map stores (scalar > isoValue) per grid point.
     */
    void computeContourBoxplotsBinaryMap();

    /**
      @brief Tries to find a default epsilon for contour boxplot computation
      which results in an overall averaged "band depth" of 1/6.

      To determine the "failing rate" we built a matrix with one entry per
      member-(member-pair)-combination. In this context a pair is formed by two
      different members taken from the set of all selected members without the
      member the pair is tested against. Different order of the members in a
      pair does NOT result in a new pair.

      Each entry of the matrix is filled with the averaged maximum value of
      intersection fails and union fails of the member-(member-pair)-combination
      the entry represents. We than count all entries with a averaged failure
      value smaller than the currently chosen epsilon and average the sum by
      the number of member-pair-combinations. This averaged sum then contains
      the averaged "band depth" which should become equal to 1/6.

      We use a binary search to determine a default epsilon. The search
      terminates early if the epsilon does not change any more or if the number
      of iterations exceeds 100000 (to prevent endless loops).
     */
    void computeContourBoxplotDefaultEpsilon();

    /**
      Computes the "band depth" value of each selected ensemble member in
      percentage of the number of selected ensemble members.

      The band depth of a member is equal to the number of pairs of members the
      member is detected to "lie in between", i.e. the percentage of grid
      points where the member fails to lie in between a pair of members
      needs to be smaller than @ref contourPlot->epsilon.

      The band depth value is combined with the ensemble member grid index
      (index * 10. + bandDepth) and stored in @ref textureCBPBandDepth sorted
      by increasing band depth value.
      
      The grid index of an ensemble member is its position in the grids vector
      of gridAggregation.
     */
    void computeContourBoxplotBandDepth();

    /**
      Computes min and max fields for both inner and outer band of a contour
      boxplot and stores the result to the four channels of
      @ref textureContourBoxplot.

      The values of the minimum fields are "mirrored" on an axis parallel to the
      x-axis and at a y-value equal to the iso value.

      The connection of channels and fields are the following:
      r, g, b, a -> minInner, maxInner, minOuter, maxOuter .
     */
    void computeContourBoxplot();

    // Computation method for contour probability plot.
    /**
      Computes min, max and median fields for inner, outer and outermost band of
      a contour probability plot and stores the result to
      @ref textureContourProbabilityPlot and contourPlot->probabilityMedian
      respectively.

      The values of the minimum fields are "mirrored" on an axis parallel to the
      x-axis and at a y-value equal to the iso value.

      Since the number of six field exceeds the number of four channels we
      choose the texture to be twice as big as the data field and to use the
      rg-channels to store min and max fields of inner and outermost band (with
      [0, dataFieldSize-1] = innerband).

      Thus the connection of channels and fields are the following:
      r,g,b,a->(minInner,minOutermost),(maxInner, maxOutermost),minOuter,maxOuter.
     */
    void computeContourProbabilityPlot();

    // Computation method for distance variability plots.
    /**
      Computes standard deviation fields and mean field per given iso value on
      the base of distance fields and stores the fields to @ref
      distanceTextureContainer and @ref distanceMean respectively.

      The distance fields to be used for computing the render textures and the
      mean field are firstly computed per ensemble member and iso value and are
      stored to @ref distanceStorage. The distance fields need to be recomputed
      only if the data field or the iso value[s] change.
     */
    void computeDistanceVariabilityPlot();

    // Computation method for scalar variability plots.
    /**
      Computes standard deviation fields and mean field on the base of the
      scalar fields of the selected ensemble members and stores the fields to
      @ref variabilityPlot and @ref scalarMean respectively.

      The iso value is subtracted from each entry of the standard deviation
      fields to set the boundary of the variability plot band to 0. By doing so
      we can use the exact same shader for rendering both distance variability
      plots and scalar variability plot.
     */
    void computeScalarVariabilityPlot();

/*------------------------------ Render Methods ------------------------------*/
    // Render method for spaghetti plots.
    void renderSpaghettiPlot(MSceneViewGLWidget *sceneView);

    // Render method for contour boxplots.
    void renderContourBoxplots(MSceneViewGLWidget *sceneView);
    void renderContourBoxplotMedianLine(MSceneViewGLWidget *sceneView);
    void renderContourBoxplotOutliers(MSceneViewGLWidget *sceneView);

    // Render method for contour probability plot.
    void renderContourProbabilityPlots(MSceneViewGLWidget *sceneView);
    void renderContourProbabilityPlotMedianLine(MSceneViewGLWidget *sceneView);

    // Render method for distance variability plots.
    void renderMultiIsoVariabilityPlot(MSceneViewGLWidget *sceneView);
    void renderMultiIsoVariabilityPlotMean(MSceneViewGLWidget *sceneView);

    // Render method for scalar variability plots.
    void renderVariabilityPlot(MSceneViewGLWidget *sceneView);
    void renderVariabilityPlotMean(MSceneViewGLWidget *sceneView);

    void deleteTexturesAndArrays();

/*--------------------------------- Shaders ----------------------------------*/

    std::shared_ptr<GL::MShaderEffect> glMarchingSquaresShader;
    std::shared_ptr<GL::MShaderEffect> glContourPlotsShader;
    std::shared_ptr<GL::MShaderEffect> glCSContourPlotsShader;
    std::shared_ptr<GL::MShaderEffect> glVariabilityPlotsShader;

/*----------------------------- Other Variables ------------------------------*/
    // texture handles for bindless textures
    GLuint64 *textureHandles;
    GLuint64 *gridTextureHandles;

    QtProperty *contourSetProperty;
    int         contourSetIndex;    // index of the contour set used
    int         numMembers;         // amount of ensemble members

    bool suppresssUpdate;
};

} // namespace Met3D

#endif // PLOTCOLLECTION_H
