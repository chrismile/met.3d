/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017 Marc Rautenhaus
**  Copyright 2017 Bianca Tost
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
#ifndef BBOXDOCKWIDGET_H
#define BBOXDOCKWIDGET_H

// standard library imports
#include <iostream>

// related third party imports
#include <QDockWidget>
#include <QTableWidget>
#include <QStyledItemDelegate>
#include <QSettings>

// local application imports

namespace Ui {
class MBoundingBoxDockWidget;
}

namespace Met3D
{

/**
  @brief Dock widget including GUI elements to handle bounding boxes.

  In detail MBoundingBoxDockWidget provides a table to display and change
  bounding box values and buttons to create and delete bounding boxes and to
  save and load configuration files.
 */
class MBoundingBoxDockWidget : public QDockWidget
{
    Q_OBJECT

public:
    explicit MBoundingBoxDockWidget(QWidget *parent = 0);
    ~MBoundingBoxDockWidget();

    QString getSettingsID() { return "BoundingBoxes";}

    /**
      @brief getSortedBoundingBoxesList returns the list of bounding boxes
      sorted in the way as it is done in the table.

      This is necessary since QMap returns a list sorted in a different way.
      (First all large letters, than all small letters; cp. Unicode table.)
      Since it might confuse the user, if the ordering of the bounding boxes in
      the selection lists of the actors differs from the ordering in the
      bounding box table, it is useful to extract the sorting from the table
      entries.
     */
    QStringList getSortedBoundingBoxesList();

    /**
      Add new bounding box and ask user to select a name.
     */
    QString addBoundingBox(const QRectF *horizontalCoords2D,
                           double bottom_hPa = 1045.,
                           double topPressure_hPa = 20.);

    /**
      Add new bounding box and using @p name as name of the bounding box.
     */
    QString addBoundingBox(QString name, const QRectF *horizontalCoords2D,
                           double bottomPressure_hPa = 1045.,
                           double topPressure_hPa = 20.);

    void saveConfiguration(QSettings *settings);

    void loadConfiguration(QSettings *settings);

    /**
      Removes all existing bounding box objects.

      Called by @ref MSessionManagerDialog::loadSessionFromFile() to ensure a
      "silent" deletion of all bounding box objects of the current session.
      Otherwise the user would be ask whether he/she wants to keep the bounding
      box objects of the current session if he/she e.g. switches the session.
     */
    void removeAllBoundingBoxes();

public slots:
    void onCellChanged(int row, int col);
    void onSpinBoxUpdate(double value);
    void onCreateBBox();
    void onCloneBBox();
    void onDeleteBBox();
    void saveConfigurationToFile(QString filename = "");
    void loadConfigurationFromFile(QString filename = "");

protected:
    void keyPressEvent(QKeyEvent *event);

private:
    /**
      @brief Handles creation of new bounding boxes.

      If @p name is an empty string or contains a name which already exists,
      it asks the user to enter a (different) name.

      @return The name of the new bounding box or "None" if user cancels
      entering a name.
     */
    QString createBoundingBox(QString name, double lon = -60., double lat = 30.,
                              double width = 100., double height = 40.,
                              double bottom = 1045., double top = 20.);
    /**
      @brief Handles creation of new bounding boxes and inserting the according
      row into the bounding box table.

      Caution: @p name needs to be a valid bounding box name (unique and not empty).
     */
    void insertRow(QString name, double lon = -60., double lat = 30.,
                      double width = 100., double height = 40.,
                      double bottom = 1045., double top = 20.);

    void updateRow(QString name, double lon, double lat, double width,
                   double height, double bottom, double top);

    QString getBBoxNameInRow(int row);

    bool isValidBoundingBoxName(QString boundingBoxName, QStringList &bBoxes,
                                bool printMessage=true);

    Ui::MBoundingBoxDockWidget *ui;

    bool suppressUpdates;
};



/**
  @brief Delegate to handle double spin boxes as part of the bounding box table.
 */
class MDoubleSpinBoxDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    MDoubleSpinBoxDelegate(MBoundingBoxDockWidget *dockWidget);

    /**
      @brief Method to create the editor as a QDoubleSpinBox with minimum,
      maxium, decimals and singleStep set to their according variables.

      Additionally, it creates a connection to @ref dockWidget to handle updates
      instantly.
     */
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    void setEditorData(QWidget *editor, const QModelIndex &index) const override;

    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;

    void updateEditorGeometry(QWidget *editor,
                              const QStyleOptionViewItem &option,
                              const QModelIndex &index) const override;

    void setMinimum(double minimum) { this->minimum = minimum; }
    void setMaximum(double maximum) { this->maximum = maximum; }
    void setDecimals(int decimals)  { this->decimals = decimals; }
    void setSingleStep(int singleStep)  { this->singleStep = singleStep; }
    void setRange(double minimum, double maximum)
    { this->minimum = minimum; this->maximum = maximum; }

private:
    double minimum;
    double maximum;
    int    decimals;
    double singleStep;

    MBoundingBoxDockWidget *dockWidget;
};


/**
  @brief Delegate to handle renaming of a bounding box correctly.
 */
class MBBoxNameDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    MBBoxNameDelegate(QObject *parent = 0) { Q_UNUSED(parent); }

    /**
      Reimplemented to check before accepting whether the name already exists.

      If the name already exists, a warning is displayed and the table entry
      is reseted to its previous value.
     */
    void setModelData(QWidget *editor, QAbstractItemModel *model,
                      const QModelIndex &index) const override;
};

} // namespace Met3D

#endif // BBOXDOCKWIDGET_H
