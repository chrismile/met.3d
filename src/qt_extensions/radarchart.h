/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2021 Christoph Neuhauser
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
#ifndef MET_3D_RADARCHART_H
#define MET_3D_RADARCHART_H

// standard library imports

// related third party imports
#include <QtCharts/QChartView>
#include <QtCharts/QPolarChart>

// local application imports

namespace QtCharts {
class QChart;
class QPolarChart;
class QCategoryAxis;
class QValueAxis;
}

namespace QtExtensions {

class MRadarChart : public QtCharts::QChartView {
public:
    explicit MRadarChart(QWidget* parent = nullptr);
    explicit MRadarChart(QtCharts::QChart* chart, QWidget* parent = nullptr);
    void setVariableNames(const QVector<QString>& names);
    void setChartTitle(const QString& chartTitle);
    void hideLegend();
    void clearRadars();
    void addRadar(const QString& radarName, const QVector<float>& variableValues);
    void addRadar(const QString& radarName, const QColor& color, const QVector<float>& variableValues);
    inline void setBackgroundVisible(bool visible) { chart->setBackgroundVisible(visible); }
    inline void setOpacity(qreal opacity) { chart->setOpacity(opacity); }

    inline QtCharts::QPolarChart* getChart() { return chart; }

private:
    void _initialize();
    QtCharts::QPolarChart* chart = nullptr;
    QtCharts::QCategoryAxis* angularAxis = nullptr;
    QtCharts::QValueAxis* radialAxis = nullptr;
    QVector<QString> variableNames;
};

}

#endif //MET_3D_RADARCHART_H
