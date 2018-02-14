/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2017-2018 Bianca Tost
**  Copyright 2017-2018 Marc Rautenhaus
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
#ifndef MSCIENTIFICDOUBLESPINBOX_H
#define MSCIENTIFICDOUBLESPINBOX_H

// standard library imports

// related third party imports
#include <QDoubleSpinBox>

// local application imports

namespace QtExtensions
{
/**
  @brief Spinbox which can display double values in standard or scientific
  notation and also can handle input in scientific or standard notation.

  Uses the scientific notation in modified normalized form to display its
  content if the absolute exponent of the scientific notation is greater than
  or equal to @ref switchNotationExp.
 */
class MScientificDoubleSpinBox : public QDoubleSpinBox
{
    Q_OBJECT

public:
    MScientificDoubleSpinBox(QWidget *parent=nullptr);
    ~MScientificDoubleSpinBox();

    int significantDigits() const;
    void setSignificantDigits(int decimals);

    int switchNotationExponent() const;
    void setSwitchNotationExponent(int switchNotationExponent);

    int minimumExponent() const;
    void setMinimumExponent(int minimumExponent);

    /**
      @brief Checks if @p input could be a double value in scientific or
      standard notation.

      1) Uses QDoubleValidator @ref validator to check if @p input represents
         a valid double value in scientific or standard notation or could become
         a valid double value by adding characters.

      2) Takes significand into account to check if exponent of @p input
         does not exceed the maximum / minimum exponent possible.

      3) Checks if the length of significand without leading and trailing zeros
         exceeds @ref varSignificantDigits.

      4) Takes exponent and significand into account to check if @p input can
         represent a value suiting minExponent().

      5) If @p input represents a valid number, checks if its value lies
         between minimum() and maximum().
     */
    virtual QValidator::State validate(QString &input, int &pos) const;

    /**
      @brief Generates a string from @p val either in standard or scientific
      notation.

      Scientific noation is used only if the absolute value of the exponent is
      greater than or equal to @ref switchNotationExp.
     */
    virtual QString textFromValue(double val) const;

    virtual double valueFromText(const QString &text) const;

public slots:
    void slotSetSwitchMinExpo(int minExp) { setSwitchNotationExponent(minExp); }
    void slotSetSignificDigits(int significDigits)
    { setSignificantDigits(significDigits); }
    void slotSetMinExpo(int dec) { setDecimals(dec); }

protected:
    QDoubleValidator *validator; // Scientiftic notation validator.
    int varSignificantDigits;  // Number of significand digits.
    int switchNotationExp; // Minimum exponent for which the scientific notation
                           // is used (checked against absolute values).
};

} // namespace QtExtensions

#endif // MSCIENTIFICDOUBLESPINBOX_H
