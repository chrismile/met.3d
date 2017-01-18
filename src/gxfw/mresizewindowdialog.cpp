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
#include "mresizewindowdialog.h"
#include "ui_resizewindowdialog.h"

// standard library imports

// related third party imports

// local application imports
#include "gxfw/mglresourcesmanager.h"


namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MResizeWindowDialog::MResizeWindowDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::MResizeWindowDialog),
    currentWidth(10),
    currentHeight(10),
    ratio(1.0),
    changeWidth(true),
    changeHeight(true)
{
    ui->setupUi(this);

    // Update actor-wise counter when the user changes the type of the
    // actor to be created.
    connect(ui->widthEdit, SIGNAL(valueChanged(int)), this,
            SLOT(adaptHeight(int)));
    connect(ui->heightEdit, SIGNAL(valueChanged(int)), this,
            SLOT(adaptWidth(int)));
    connect(ui->keepRatio, SIGNAL(toggled(bool)), this,
            SLOT(updateRatio(bool)));
    connect(ui->resetButton, SIGNAL(clicked(bool)), this,
            SLOT(resetEdits()));
}


MResizeWindowDialog::~MResizeWindowDialog()
{
    delete ui;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MResizeWindowDialog::setup(int width, int height)
{
    currentWidth = width;
    currentHeight = height;
    ui->widthEdit->setValue(width);
    ui->heightEdit->setValue(height);
    ratio = (double)width / (double)height;
}


int MResizeWindowDialog::getWidth()
{
    return ui->widthEdit->value();
}


int MResizeWindowDialog::getHeight()
{
    return ui->heightEdit->value();
}


void MResizeWindowDialog::setWidth(int width)
{
    ui->widthEdit->setValue(width);
}


void MResizeWindowDialog::setHeight(int height)
{
    ui->heightEdit->setValue(height);
}


void MResizeWindowDialog::setRatio(int width, int height)
{
    ratio = (double)width / (double)height;
}


/******************************************************************************
***                          PROTECTED METHODS                              ***
*******************************************************************************/


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

void MResizeWindowDialog::adaptWidth(int height)
{
    if (ui->keepRatio->isChecked())
    {
        int width = (int)(height * ratio);
        // Avoid getting trapped by setValue() changing the value back again.
        if (changeWidth)
        {
            changeHeight = false;
            ui->widthEdit->setValue(width);
        }
        else
        {
            changeWidth = true;
        }
    }
}


void MResizeWindowDialog::adaptHeight(int width)
{
    if (ui->keepRatio->isChecked())
    {
        int height = (int)(width * 1.0/ratio);
        // Avoid getting trapped by setValue() changing the value back again.
        if (changeHeight)
        {
            changeWidth = false;
            ui->heightEdit->setValue(height);
        }
        else
        {
            changeHeight = true;
        }
    }
}


void MResizeWindowDialog::updateRatio(bool toggeled)
{
    if (toggeled)
    {
        ratio = (double)ui->widthEdit->value()/(double)ui->heightEdit->value();
    }
}


void MResizeWindowDialog::resetEdits()
{
    // Set ratio to current window size ratio.
    ratio = (double)currentWidth / (double)currentHeight;
    // Prevent ratio adaption if necessary.
    if (ui->keepRatio->isChecked())
    {
        changeWidth = false;
        changeHeight = false;
    }
    // Reset width and height to current window size.
    ui->widthEdit->setValue(currentWidth);
    ui->heightEdit->setValue(currentHeight);
}

} // namespace Met3D
