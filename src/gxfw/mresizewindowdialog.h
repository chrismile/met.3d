/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
**  Copyright 2015 Bianca Tost
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

#ifndef MRESIZEWINDOWDIALOG_H
#define MRESIZEWINDOWDIALOG_H

// standard library imports
#include <map>

// related third party imports
#include <QDialog>

// local application imports
#include <gxfw/mactor.h>


namespace Ui
{
class MResizeWindowDialog;
}

namespace Met3D
{


class MResizeWindowDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MResizeWindowDialog(QWidget *parent = 0);

    ~MResizeWindowDialog();

    int getWidth();

    int getHeight();

    void setWidth(int width);

    void setHeight(int height);

    void setRatio(int width, int height);

    /**
      Sets @ref currentWidth, @ref currentHeight and default values of
      @ref Ui::MResizeWindowDialog::widthEdit and
      @ref Ui::MResizeWindowDialog::heightEdit to width and height respectively.
      Needs to be called before the dialog is executed.
     */
    void setup(int width, int height);

protected:

private slots:
    /**
      Adapts value of @ref Ui::MResizeWindowDialog::widthEdit to value of
      @ref Ui::MResizeWindowDialog::heightEdit with respect to
      @ref ratio if @ref Ui::MResizeWindowDialog::keepRatio is toggeled.
     */
    void adaptWidth(int height);

    /**
      Adapts value of @ref Ui::MResizeWindowDialog::heightEdit.
      @see adaptWidth()
     */
    void adaptHeight(int width);

    /**
      Updates @ref ratio to the ratio of the current values given by
      @ref Ui::MResizeWindowDialog::widthEdit and
      @ref Ui::MResizeWindowDialog::heightEdit if
      @ref Ui::MResizeWindowDialog::keepRatio is toggeled.
     */
    void updateRatio(bool toggeled);

    /**
      Resets the edits to currentWidth and currentHeight respectively.
     */
    void resetEdits();

private:

    Ui::MResizeWindowDialog *ui;
    int currentWidth;
    int currentHeight;

    /**
      Holds the ratio (= width/height) of the values given by the edits as
      @ref Ui::MResizeWindowDialog::keepRatio was clicked.
     */
    double  ratio;

    /**
      Indicators to show whether a call to one of the adapt methods may change
      the value. Necessary to avoid getting trapped by setValue() changing the
      value back again.
     */
    bool    changeWidth;
    bool    changeHeight;
};

} // namespace Met3D
#endif // MRESIZEWINDOWDIALOG_H
