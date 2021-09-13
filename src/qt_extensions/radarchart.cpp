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
#include "radarchart.h"

// standard library imports
#include <cmath>
#include <iostream>

// related third party imports
#include <QWidget>
#include <QtCharts/QAbstractAxis>
#include <QtCharts/QValueAxis>
#include <QtCharts/QLineSeries>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QCategoryAxis>
#include <QtCharts/QLegendMarker>

// local application imports

namespace QtExtensions {

MRadarChart::MRadarChart(QWidget* parent) : QtCharts::QChartView(parent)
{
    _initialize();
}

MRadarChart::MRadarChart(QtCharts::QChart* chart, QWidget* parent) : QtCharts::QChartView(chart, parent)
{
    _initialize();
}

void MRadarChart::_initialize()
{
    chart = new QtCharts::QPolarChart();
    this->setChart(chart);

    chart->setBackgroundBrush(QBrush(QColor(220, 220, 220, 90)));
    //chart->setPlotAreaBackgroundVisible(false);
    //chart->setPlotAreaBackgroundBrush(QBrush(QColor("transparent")));
    chart->setPlotAreaBackgroundVisible(true);
    chart->setPlotAreaBackgroundBrush(QBrush(QColor(240, 240, 240, 140)));
    chart->setMargins(QMargins(0, 0, 0, 0));
    chart->setContentsMargins(0, 0, 0, 0);
    chart->setMargins(QMargins(20, 0, 20, 0));
    chart->setBackgroundRoundness(8.0f);

    this->setBackgroundBrush(QBrush(QColor("transparent")));
    this->setContentsMargins(QMargins(0, 0, 0, 0));
    this->setAutoFillBackground(false);
    this->viewport()->setAutoFillBackground(false);

    QPalette palette = this->palette();
    palette.setBrush(QPalette::Base, Qt::transparent);
    this->setPalette(palette);
    this->setAttribute(Qt::WA_OpaquePaintEvent, false);
    this->setAttribute(Qt::WA_Hover, true);
    this->setEnabled(false);

    chart->legend()->hide();

    this->resize(300, 300);
    //this->setMaximumSize(600, int(600.0f * 0.8f));
}

void MRadarChart::setVariableNames(const QVector<QString>& names)
{
    if (angularAxis) {
        delete angularAxis;
    }
    if (radialAxis) {
        delete radialAxis;
    }

    variableNames = names;

    angularAxis = new QtCharts::QCategoryAxis();
    angularAxis->setLabelsPosition(QtCharts::QCategoryAxis::AxisLabelsPositionOnValue);
    angularAxis->setRange(0, variableNames.size());
    for (int i = 0; i < variableNames.size(); i++) {
        angularAxis->append(variableNames.at(i), i);
    }
    angularAxis->setShadesVisible(true);
    angularAxis->setShadesBrush(QBrush(QColor(249, 249, 255)));
    angularAxis->setShadesPen(Qt::NoPen);
    QFont font = angularAxis->labelsFont();
    std::cout << font.pointSize() << std::endl;
    font.setPointSize(int(std::round(font.pointSize() * 0.5f)));
    angularAxis->setLabelsFont(font);
    angularAxis->setLabelsPosition(QtCharts::QCategoryAxis::AxisLabelsPositionCenter);
    //         AxisLabelsPositionCenter = 0x0,
    //        AxisLabelsPositionOnValue = 0x1
    chart->addAxis(angularAxis, QtCharts::QPolarChart::PolarOrientationAngular);

    radialAxis = new QtCharts::QValueAxis();
    radialAxis->setTickCount(variableNames.size() + 1);
    //radialAxis->setLabelFormat("%.1f");
    radialAxis->setLabelFormat("@");
    chart->addAxis(radialAxis, QtCharts::QPolarChart::PolarOrientationRadial);
}

void MRadarChart::setChartTitle(const QString& chartTitle)
{
    chart->setTitle(chartTitle);
}

void MRadarChart::hideLegend()
{
    chart->legend()->hide();
}

void MRadarChart::clearRadars()
{
    chart->removeAllSeries();
}

void MRadarChart::addRadar(uint32_t id, const QString& radarName, const QVector<float>& variableValues)
{
    const std::vector<QColor> predefinedColors = {
            // RED
            QColor(228, 26, 28),
            // BLUE
            QColor(55, 126, 184),
            // GREEN
            QColor(5, 139, 69),
            // PURPLE
            QColor(129, 15, 124),
            // ORANGE
            QColor(217, 72, 1),
            // PINK
            QColor(231, 41, 138),
            // GOLD
            QColor(254, 178, 76),
            // DARK BLUE
            QColor(0, 7, 255)
    };

    QColor color = predefinedColors[(chart->series().size() / 2) % predefinedColors.size()];
    addRadar(id, radarName, color, variableValues);
}

void MRadarChart::addRadar(
        uint32_t id, const QString& radarName, const QColor& color, const QVector<float>& variableValues)
{
    QtCharts::QLineSeries* seriesLines = new QtCharts::QLineSeries();
    seriesLines->setName(radarName);
    for (int i = 0; i <= variableNames.size(); i++) {
        seriesLines->append(i, variableValues.at(i % variableValues.size()));
    }

    QtCharts::QLineSeries* seriesLower = new QtCharts::QLineSeries();
    for (int i = 0; i <= variableNames.size(); i++) {
        seriesLower->append(i, 0.0f);
    }

    QPen pen;
    pen.setWidth(3);
    pen.setBrush(color);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    seriesLines->setPen(pen);

    QtCharts::QAreaSeries *areaSeries = new QtCharts::QAreaSeries();
    areaSeries->setUpperSeries(seriesLines);
    areaSeries->setLowerSeries(seriesLower);
    areaSeries->setOpacity(0.2);
    areaSeries->setBrush(QBrush(color));

    chart->addSeries(seriesLines);
    chart->addSeries(areaSeries);

    SeriesTempData seriesTempData;
    seriesTempData.seriesLines = seriesLines;
    seriesTempData.seriesLower = seriesLower;
    seriesTempData.areaSeries = areaSeries;
    seriesMap.insert(std::make_pair(id, seriesTempData));

    auto markersList = chart->legend()->markers();
    markersList[markersList.size() - 1]->setVisible(false);

    seriesLines->attachAxis(radialAxis);
    seriesLines->attachAxis(angularAxis);
    areaSeries->attachAxis(radialAxis);
    areaSeries->attachAxis(angularAxis);
}

void MRadarChart::removeRadar(uint32_t id)
{
    auto it = seriesMap.find(id);
    chart->removeSeries(it->second.seriesLines);
    chart->removeSeries(it->second.seriesLower);
    chart->removeSeries(it->second.areaSeries);
    delete it->second.seriesLines;
    delete it->second.seriesLower;
    delete it->second.areaSeries;
    seriesMap.erase(id);
}

void MRadarChart::enterEvent(QEvent* event)
{
    setOpacity(0.2f);
    QWidget::enterEvent(event);
}

void MRadarChart::leaveEvent(QEvent* event)
{
    setOpacity(1.0f);
    QWidget::leaveEvent(event);
}

bool MRadarChart::hasHeightForWidth() const
{
    return true;
}

int MRadarChart::heightForWidth(int w) const
{
    return int(float(w) * 0.8f);
}

MMultiVarChartCollection::MMultiVarChartCollection() {
    _initialize();
}

MMultiVarChartCollection::MMultiVarChartCollection(QWidget* parent) : QGridLayout(parent) {
    _initialize();
}

void MMultiVarChartCollection::_initialize() {
    verticalSpacer = new QSpacerItem(
            20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);
    addItem(verticalSpacer, 0, 0, 1, 1);
    horizontalSpacer = new QSpacerItem(
            40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);
    addItem(horizontalSpacer, 1, 0);
}

void MMultiVarChartCollection::addChartView(QtCharts::QChartView* chartView) {
    removeItem(horizontalSpacer);

    addWidget(chartView, 1, charts.size());
    charts.push_back(chartView);
    addItem(horizontalSpacer, 1, charts.size());
}

void MMultiVarChartCollection::clear() {
    for (QtCharts::QChartView* radarChart : charts) {
        this->removeWidget(radarChart);
    }
    charts.clear();

    removeItem(horizontalSpacer);
    addItem(horizontalSpacer, 1, 0);
}

}
