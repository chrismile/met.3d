//
// Created by kaiser on 16.04.17.
//

#ifndef MET_3D_ABSTRACTDATACALCULATOR_H
#define MET_3D_ABSTRACTDATACALCULATOR_H

// standard library imports

// related third party imports
#include <QtCore>

// local application imports
#include "data/weatherpredictiondatasource.h"


namespace Met3D
{

/**
 * @brief MAbstractDataCalculator is the base class for all data calculation
 */
class MAbstractDataCalculator
{
public:
    MAbstractDataCalculator(QString identifier);
    virtual ~MAbstractDataCalculator();

    /**
     * Returns the identifier string of this data calculator.
     */
    QString getIdentifier() { return identifier; }

    /**
     * Set input source for calculations
     * @param reader is a pointer to a MWeatherPredictionDataSource instance
     */
    void setInputSource(MWeatherPredictionDataSource* source);

protected:

    /**
     * Implementations of this class should check the input dataSource
     */
    virtual void checkDataSource() = 0;

    QString identifier;
    MWeatherPredictionDataSource* dataSource;

};


} // namespace Met3D

#endif //MET_3D_ABSTRACTDATACALCULATOR_H
