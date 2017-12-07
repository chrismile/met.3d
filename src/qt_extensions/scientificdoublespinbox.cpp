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
#include "scientificdoublespinbox.h"

// standard library imports
#include <float.h>
#include <iostream>

// related third party imports

// local application imports

namespace QtExtensions
{

MScientificDoubleSpinBox::MScientificDoubleSpinBox(QWidget *parent) :
    QDoubleSpinBox(parent),
    varSignificantDigits(2),
    switchNotationExp(1)
{
    validator = new QDoubleValidator();
    validator->setNotation(QDoubleValidator::ScientificNotation);
}


MScientificDoubleSpinBox::~MScientificDoubleSpinBox()
{
    delete validator;
}


int MScientificDoubleSpinBox::significantDigits() const
{
    return varSignificantDigits;
}


void MScientificDoubleSpinBox::setSignificantDigits(int decimals)
{
    varSignificantDigits = qBound(1, decimals, 9);
    setValue(value());
}


int MScientificDoubleSpinBox::switchNotationExponent() const
{
    return switchNotationExp;
}


void MScientificDoubleSpinBox::setSwitchNotationExponent(int switchNotationExponent)
{
    switchNotationExp = qBound(0, switchNotationExponent, DBL_MAX_10_EXP);
    setValue(value());
}


int MScientificDoubleSpinBox::minimumExponent() const
{
    return decimals();
}


void MScientificDoubleSpinBox::setMinimumExponent(int minimumExponent)
{
    setDecimals(minimumExponent);
}


QValidator::State MScientificDoubleSpinBox::validate(QString &text,
                                                     int &pos) const
{
    QString numberString = text;
    // Remove prefix and suffix of the text string to analyse the spin box entry
    // only.
    numberString.chop(suffix().length());
    numberString.remove(0, prefix().length());
    QValidator::State state = validator->validate(numberString, pos);
    if (state != QValidator::Invalid)
    {
        // Remove group seperators since they don't have impact on the actual
        // value of the number but might cause problems when parsing the string
        // e.g. when testing for trailing non-zero values.
        numberString.remove(locale().groupSeparator());
        // If the string is empty or only consists of group seperators, no
        // further parsing is necessary.
        if (numberString.isEmpty())
        {
            return QValidator::Intermediate;
        }
        int exponent = 0;
        QString significandString = numberString;
        // Check if the exponent is valid, i.e. is not too big or too small.
        // (regEx: [eE][-+]*\d+). Using of the asterix to include the sign
        // characters is valid since the validator makes sure that at most one
        // sign is present after the exponential sign.
        QRegExp regularExp = QRegExp(
                    QString(locale().exponential()) + "[\\"
                    + locale().negativeSign() + "\\" + locale().positiveSign()
                    + "]*\\d+", Qt::CaseInsensitive);
        if (numberString.contains(regularExp))
        {
            QString powerString = regularExp.capturedTexts().at(0);
            significandString.chop(powerString.length());
            // Remove the base from the power string to get the exponent.
            powerString.remove(QRegExp(QString(locale().exponential()),
                                       Qt::CaseInsensitive));
            exponent = powerString.toInt();
            // If present, take also the significand into account since it might
            // impact the actual exponent.
            if (significandString.length() > 0)
            {
                double significand = locale().toDouble(significandString);
                // Skip adaption step for significand equal to zero since the
                // logarithm of 0 is not defined but a significand equal to zero
                // has no impact on the exponent.
                if (significand != 0.)
                {
                    // Adapt the exponent by the value of the significand.
                    exponent += static_cast<int>(floor(log10(fabs(significand))));
                }
            }
            if (exponent < -minimumExponent()
                    || exponent <= DBL_MIN_10_EXP || exponent >= DBL_MAX_10_EXP)
            {
                return QValidator::Invalid;
            }
            exponent = powerString.toInt();
        }
        else
        {
            // The exponent might be partially present so remove the exponential
            // sign and everything after it to get the significand.
            int index = significandString.indexOf(locale().exponential(), 0,
                                                  Qt::CaseInsensitive);
            if (index >= 0)
            {
                significandString.chop(significandString.length() - index);
            }
        }

        // Only allow a significand with a length of as most equal to
        // varSignificantDigits. Leading and trailing zeros have no impact on
        // the length of the significand.
        int indexFirstSignificNumber = significandString.indexOf(
                    QRegExp(QString("[^0\\") + locale().decimalPoint() + "]"));
        if (indexFirstSignificNumber >= 0)
        {
            int indexLastSignificNumber = significandString.lastIndexOf(
                        QRegExp(QString("[^0\\") + locale().decimalPoint() + "]"));
            int indexDecimalPoint = significandString.indexOf(
                        locale().decimalPoint());
            int significandLength =
                    indexLastSignificNumber - indexFirstSignificNumber + 1;
            if (indexDecimalPoint > indexFirstSignificNumber
                    && indexDecimalPoint < indexLastSignificNumber)
            {
                significandLength--;
            }
            if (significandLength > varSignificantDigits)
            {
                return QValidator::Invalid;
            }
        }

        // To check if the value suits the desired decimals the fractional part
        // of the value is examined for non-zero values "after" the desired
        // amount of decimal places. Since the exponent affects the actual
        // fractional part, we need to consider it when examing the
        // significand's fractional part.
        if (significandString.length() > 0)
        {
            int decimalPointIndex = significandString.indexOf(
                        QString(locale().decimalPoint()));
            // Remove decimal point from the string so it won't affect
            // extracting the fractional part.
            significandString.remove(locale().decimalPoint());
            // Determine the length of the substring containing the fractional
            // part after applying the exponent.
            int fractionalPartLength = 0;
            if (decimalPointIndex < 0)
            {
                // If the signifiand does not have a fractional part, a
                // fractional part might be "generated" by the exponent.
                fractionalPartLength = -exponent;
            }
            else
            {
                // Combine fractional part and exponent to get the actual
                // fractional part of the number.
                fractionalPartLength = significandString.length()
                        - (decimalPointIndex + exponent);
            }
            // Avoid negative length.
            fractionalPartLength = std::max(0, fractionalPartLength);
            // Extract the fractional part from the significand.
            QString fractionalPart = significandString.right(
                        fractionalPartLength);            
            // Remove the "allowed" decimal places to get the fractional part
            // smaller than the minimum exponent.
            // Special case: If the exponent "exceeds" the significand by
            // "moving" the decimal point to the left, it's like appending zeros
            // to the fractional part. Since it is unnecessary to append
            // zeros for real this effect is simulated by removing less decimal
            // places to get the fractional part after the "allowed" decimal
            // places.
            if (fractionalPartLength > fractionalPart.length())
            {
                fractionalPart.remove(0, std::max(0, minimumExponent()
                                                  - (fractionalPartLength
                                                     - fractionalPart.length())));
            }
            else
            {
                fractionalPart.remove(0, minimumExponent());
            }
            // Test if the number is valid, i.e. decimal places "after" the
            // "allowed" decimal places are all equal to zero.
            if (fractionalPart.contains(QRegExp("[1-9]")))
            {
                return QValidator::Invalid;
            }
        }
        if (state == QValidator::Acceptable)
        {
            bool ok;
            double value = locale().toDouble(numberString, &ok);
            if (!ok || value > maximum())
            {
                return QValidator::Invalid;
            }
            if (value < minimum())
            {
                // Don't reject values smaller than minimum since otherwise the
                // user might not be able to enter a value (e.g. if the minimum
                // value is greater than 10.)
                return QValidator::Intermediate;
            }
        }
    }
    return state;
}


QString MScientificDoubleSpinBox::textFromValue(double value) const
{
    int significDigits = varSignificantDigits - 1;

    QString text = QLocale::system().toString(value, 'E', significDigits);
    double valueFromString = locale().toDouble(text);

    // Switch to scientific notation only if the absolute value of the exponent
    // is bigger than the threshold given.
    if (valueFromString != 0.
            && (static_cast<int>(fabs(floor(log10(fabs(value)))))
                >= switchNotationExp))
    {
        text = QLocale::system().toString(value, 'E', significDigits);
    }
    else
    {
        // Since the toString-method takes decimal places but not significand
        // digits as argument, it is necessary to allow more decimals for
        // decimal numbers in standard notation with leading zeros.
        int indexExponentialSign = text.indexOf(
                    locale().exponential(), 0, Qt::CaseInsensitive);
        QString exponentString = text.right(
                    text.length() - (indexExponentialSign + 1));
        int exponent = locale().toInt(exponentString);
        if (exponent < 0)
        {
            significDigits -= exponent;
        }

        text = locale().toString(valueFromString, 'f',
                                         significDigits);
    }

    if (qAbs(value) >= 1000.)
    {
        text.remove(locale().groupSeparator());
    }

    // Remove trailing zeros after decimal point.
    int decimalPointIndex = text.indexOf(QLocale::system().decimalPoint());
    if (decimalPointIndex > 0)
    {
        int exponentIndex = text.indexOf(QLocale::system().exponential(), 0,
                                         Qt::CaseInsensitive);
        if (exponentIndex >= 0)
        {
            exponentIndex -= 1;
        }
        else
        {
            exponentIndex = text.length();
        }
        int nonZeroIndex = text.lastIndexOf(QRegExp("[^0]"), exponentIndex);
        if (nonZeroIndex >= decimalPointIndex)
        {
            if (nonZeroIndex != decimalPointIndex)
            {
                nonZeroIndex++;
            }
            text.remove(nonZeroIndex, exponentIndex + 1 - nonZeroIndex);
        }
    }

    return text;
}

double MScientificDoubleSpinBox::valueFromText(const QString &text) const
{
    return locale().toDouble(text);
}

} // namespace QtExtensions
