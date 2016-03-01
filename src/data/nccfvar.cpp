/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2015 Marc Rautenhaus
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
#include "nccfvar.h"

// standard library imports

// related third party imports
#include <QRegExp>
#include <QString>
#include <QStringList>

// local application imports
#include "util/mutil.h"

using namespace std;
using namespace netCDF;
using namespace netCDF::exceptions;


/******************************************************************************
***                         UTILITY ROUTINES                                ***
*******************************************************************************/

/**
  Fixes a C++ string object that still contains a zero termination character at
  the end. Some attributes read from NetCDF files can show such a behaviour.
  For instance, the time units attribute read with a statement such as 'string
  attribute; var.getAtt("units").getValues(attribute);' could still have a '0'
  value at the last position.

  This method checks if the last character of the given string s is a valid
  ASCII character (value >= 32). If not, the last character is removed.
  */
inline void fixZeroTermination(string *s)
{
    if (s->length() > 0)
        if (s->at(s->length()-1) < 32) s->resize(s->length()-1);
}


/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

NcCFVar::NcCFVar()
    : NcVar()
{
    // The following regular expression matches valid time units strings from
    // the "units" attribute of the time variable.
    // Cf. http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/cf-conventions.html#time-coordinate.
    // The groups of the expression are set so that they extract date and time
    // etc. (mr, 14Sept2011).

    // RegExp that works in Python:
    // "(second|sec|s|minute|min|hour|hr|h|day|d|year|yr)s? since (\d+)-(\d+)-(\d+)(?:[T\s](\d+)(?::(\d+)(?::(\d+(?:\.\d+)?))?)?)?[Z\s]?(?:UTC|(-?\d+):?(\d+)?)?\Z"

    // (From the Qt documentation:) Note: The C++ compiler transforms
    // backslashes in strings. To include a \ in a regexp, enter it twice, i.e.
    // \\. To match the backslash character itself, enter it four times, i.e.
    // \\\\.
    // Because QRegExp is string oriented, there are no \A, \Z, or \z
    // assertions. The \G assertion is not supported but can be emulated in a
    // loop.

    // RegExp that works in C++ (\ -> \\; \Z -> $):
    reTimeUnits.setPattern("(second|sec|s|minute|min|hour|hr|h|day|d|year|yr)s? since (\\d+)-(\\d+)-(\\d+)"
                           "(?:[T\\s](\\d+)(?::(\\d+)(?::(\\d+(?:\\.\\d+)?))?)?)?[Z\\s]?"
                           "(?:UTC|(-?\\d+):?(\\d+)?)?$");
    reTimeUnits.setCaseSensitivity(Qt::CaseInsensitive);

    varGridType = UNDEFINED;
}


NcCFVar::NcCFVar(const NcVar& rhs)
    : NcVar(rhs)
{
    reTimeUnits.setPattern("(second|sec|s|minute|min|hour|hr|h|day|d|year|yr)s? since (\\d+)-(\\d+)-(\\d+)"
                           "(?:[T\\s](\\d+)(?::(\\d+)(?::(\\d+(?:\\.\\d+)?))?)?)?[Z\\s]?"
                           "(?:UTC|(-?\\d+):?(\\d+)?)?$");
    reTimeUnits.setCaseSensitivity(Qt::CaseInsensitive);

    varGridType = UNDEFINED;
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

NcVar NcCFVar::getCFCoordinateVar(const vector<string>& units,
                                  const string& standardname,
                                  bool requirepositive) const
{
    string attribute;

    // Loop over all coordinate (=dimension) variables of this variable.
    for (int i = 0; i < getDimCount(); i++) {
        NcVar var = getParentGroup().getVar(getDim(i).getName());

        // If 'requirepositive' is true, the coordinate variable has to have
        // the 'positive' attribute. This is relevant for vertical dimensions,
        // cf.
        // http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/cf-conventions.html#vertical-coordinate
        if (requirepositive) {
            try {
                var.getAtt("positive").getValues(attribute);
                // The 'positive' attribute is present but contains a value
                // other that 'up' or 'down': Continue with next variable.
                if (!((attribute == "up") || (attribute == "down"))) continue;
            }
            // An NcException here means that the 'positive' attribute is not
            // defined for the variable. Continue with next variable.
            catch (NcException) {
                continue;
            }
        }

        // Try to match one of the values of the 'units' vector to the units
        // attribute of the variable, if available.
        try {
            var.getAtt("units").getValues(attribute);
            for (unsigned int i=0; i<units.size(); i++)
                if (attribute == units[i]) return var;
        }
        // An NcException here means that the 'units' attribute is not defined
        // for the variable. Skip.
        catch (NcException) {}

        // Test if the standard name of the variable (if available) equals the
        // standard name we're looking for.
        try {
            var.getAtt("standard_name").getValues(attribute);
            if (attribute == standardname) return var;
        }
        catch (NcException) {}
    }

    // If we get here no variable has been identified. Throw an exception.
    throw NcException("NcException", "CF coordinate variable '" + standardname
                      + "' not found", __FILE__, __LINE__);
}


NcVar NcCFVar::getLatitudeVar()
{
    // List of units from which the latitude variable can be recognised
    // (http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/cf-conventions.html#latitude-coordinate).
    // (NOTE: extended initialiser lists require the C++-0x standard).
    vector<string> units = {"degrees_north", "degree_north", "degree_N",
                            "degrees_N", "degreeN", "degreesN"};

    // Find a variable whose 'units' attribute equals one of the specified
    // values or whose 'standard_name' attribute equals 'latitude'.
    return getCFCoordinateVar(units, "latitude");
}


NcVar NcCFVar::getLongitudeVar()
{
    // List of units from which the longitude variable can be recognised
    // (http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/cf-conventions.html#longitude-coordinate).
    // (NOTE: extended initialiser lists require the C++-0x standard).
    vector<string> units = {"degrees_east", "degree_east", "degree_E",
                            "degrees_E", "degreeE", "degreesE"};

    // Find a variable whose 'units' attribute equals one of the specified
    // values or whose 'standard_name' attribute equals 'longitude'.
    return getCFCoordinateVar(units, "longitude");
}


NcVar NcCFVar::getVerticalCoordinatePressure()
{
    // The vertical pressure coordinate is identifyable by units of pressure, cf.
    // http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/cf-conventions.html#vertical-coordinate
    vector<string> units = {"Pa", "hPa", "bar", "millibar",
                            "decibar", "atmosphere", "atm"};
    return getCFCoordinateVar(units, "", true);
}


NcVar NcCFVar::getVerticalCoordinateHybridSigmaPressure(NcVar *ap, NcVar *b,
                                                        QString* psfcName)
{
    // http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/cf-conventions.html#dimensionless-vertical-coordinate
    vector<string> units = {"level", "layer", "sigma_level", ""};
    NcVar hybridVar = getCFCoordinateVar(
                units, "atmosphere_hybrid_sigma_pressure_coordinate", true);

    // Identify the variables that accomodate the ap and b coefficients for
    // computing model level pressure. The variable names are stored in
    // the "formula_terms" attribute, as described in the CF conventions:
    // http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/cf-conventions.html#dimensionless-v-coord
    string formulaTerms = "";
    try { hybridVar.getAtt("formula_terms").getValues(formulaTerms); }
    catch (NcException) {}

    // Set up a regular expression to match the "formula_terms" attribute.
    QRegExp re("ap: (.+) b: (.+) ps: (.+)");

    // If the formula_terms string cannot be matched to the regular expression,
    // don't modify the variables for ap and b. Otherwise..
    if (re.indexIn(QString::fromStdString(formulaTerms)) >= 0)
    {
        // .. get the parsed groups (1 = variable name ap, 2 = b, 3 = surface
        // pressure) ..
        QStringList parsedValues = re.capturedTexts();
        // .. and get the corresponding ap and b variables:
        *ap = getParentGroup().getVar(parsedValues.at(1).toStdString());
        *b  = getParentGroup().getVar(parsedValues.at(2).toStdString());
        *psfcName = parsedValues.at(3);
    }

    return hybridVar;
}


NcVar NcCFVar::getVerticalCoordinatePotVort()
{
    vector<string> units = {"10-6Km2/kgs"};
    return getCFCoordinateVar(units, "", true);
}


bool NcCFVar::hasEnsembleDimension()
{
    try
    {
        NcVar dummy = getEnsembleVar();
        return true;
    }
    catch (NcException)
    {
        return false;
    }
}


NcVar NcCFVar::getEnsembleVar()
{
    // NOTE: The ensemble dimension currently (01Oct2012) doesn't seem to be
    // part of the CF-1.6 conventions. There is no standard name and no
    // attributes, hence our only chance is to recognize it by its name.
    // The netcdf-java 4.3 name is "ens0". A made-up standard name is
    // ensemble_member_id.

    // Loop over all coordinate (=dimension) variables of this variable.
    for (int i = 0; i < getDimCount(); i++)
    {
        NcVar var = getParentGroup().getVar(getDim(i).getName());
        // Match name ...
        if (var.getName() == "ens0") return var;
        // ... or standard name.
        try
        {
            string attribute;
            var.getAtt("standard_name").getValues(attribute);
            fixZeroTermination(&attribute);
            if (attribute == "ensemble_member_id") return var;
        }
        catch (NcException) {}
    }

    // If we get here no variable has been identified. Throw an exception.
    throw NcException("NcException", "cannot identify ensemble variable",
                      __FILE__, __LINE__);

}


QSet<unsigned int> NcCFVar::getEnsembleMembers()
{
    NcVar ensVar = getEnsembleVar();

    // Load ensemble data.
    unsigned int numMembers = ensVar.getDim(0).getSize();
    int *ensValues = new int[numMembers];
    ensVar.getVar(ensValues);

    QSet<unsigned int> members;
    for (unsigned int i = 0; i < numMembers; i++) members.insert(ensValues[i]);
    delete[] ensValues;

    return members;
}


bool NcCFVar::hasTimeDimension()
{
    try
    {
        NcVar dummy = getTimeVar();
        return true;
    }
    catch (NcException)
    {
        return false;
    }
}


NcVar NcCFVar::getTimeVar()
{
    // If this method has been run successfully before, return the stored
    // variable.
    if (!timeVar.isNull()) return timeVar;

    // Loop over all coordinate (=dimension) variables of this variable.
    for (int i = 0; i < getDimCount(); i++)
    {
        NcVar var = getParentGroup().getVar(getDim(i).getName());
        // Check if the variable for the current dimension is defined. Every
        // dimension should correspond to a variable in CF compliant files.
        if (var.isNull())
        {
            continue;
        }

        try {
            string attribute;
            var.getAtt("units").getValues(attribute);
            fixZeroTermination(&attribute);
            // Try to match the units string with the regular expression
            // describing valid units string. If the match is successful, this
            // is our time variable.
            if (reTimeUnits.indexIn(QString::fromStdString(attribute)) >= 0)
            {
                timeVar = var;
                return var;
            }
        }
        // An NcException here means that the 'units' attribute is not defined
        // for the variable. Skip.
        catch (NcException) {}
    }

    // If we get here no variable has been identified. Throw an exception.
    throw NcException("NcException", "cannot identify time variable",
                      __FILE__, __LINE__);
}


QList<QDateTime> NcCFVar::getTimeValues()
{
    NcVar timeVar = getTimeVar();

    // Parse the time units from the "units" attribute.
    string units;
    timeVar.getAtt("units").getValues(units);
    fixZeroTermination(&units);

    QDateTime baseTime;
    double utcOffset = 0.0;
    QString timeUnit;
    parseTimeUnits(QString::fromStdString(units), &baseTime,
                   &utcOffset, &timeUnit);

    // Load raw time data.
    unsigned int size = timeVar.getDim(0).getSize();
    double *timeValues = new double[size];
    timeVar.getVar(timeValues);

    // Convert time data to QDateTime objects. The "round()" call is in
    // particular necessary to get the correct time for day or year units (e.g.
    // in EMAC and ECHAM datasets).
    QList<QDateTime> convertedTimeValues;
    for (unsigned int i = 0; i < size; i++)
    {
        if (timeUnit == "second")
        {
            int secs = int(round(timeValues[i]));
            convertedTimeValues.append(baseTime.addSecs(secs));
        }
        else if (timeUnit == "minute")
        {
            // Break into days and seconds to prevent overflows.
            int days = int(timeValues[i] / 1440.);
            int secs = round(fmod(timeValues[i], 1440.) * 60.);
            QDateTime t = baseTime.addDays(days);
            convertedTimeValues.append(t.addSecs(secs));
        }
        else if (timeUnit == "hour")
        {
            int days = int(timeValues[i] / 24.);
            int secs = round(fmod(timeValues[i], 24.) * 3600.);
            QDateTime t = baseTime.addDays(days);
            convertedTimeValues.append(t.addSecs(secs));
        }
        else if (timeUnit == "day")
        {
            int days = int(timeValues[i]);
            int secs = round(fmod(timeValues[i], 1.) * 86400.);
            QDateTime t = baseTime.addDays(days);
            convertedTimeValues.append(t.addSecs(secs));
        }
        else if (timeUnit == "year")
        {
            int years = int(timeValues[i]);
            int secs = round(fmod(timeValues[i], 1.) * 304560000.);
            QDateTime t = baseTime.addYears(years);
            convertedTimeValues.append(t.addSecs(secs));
        }
        else
            throw NcException(
                    "NcException",
                    "cannot identify time unit "+timeUnit.toStdString(),
                    __FILE__, __LINE__);
    }

    delete[] timeValues;
    return convertedTimeValues;
}


QDateTime NcCFVar::getBaseTime()
{
    // Parse the time units from the "units" attribute.
    NcVar timeVar = getTimeVar();
    string units;
    timeVar.getAtt("units").getValues(units);
    fixZeroTermination(&units);

    QDateTime baseTime;
    double utcOffset = 0.0;
    QString timeUnit;
    parseTimeUnits(QString::fromStdString(units), &baseTime,
                   &utcOffset, &timeUnit);

    return baseTime;
}


QDateTime NcCFVar::getTimeFromAttribute(QString attributeName)
{
    string att;
    getAtt(attributeName.toStdString()).getValues(att);
    fixZeroTermination(&att);

    QDateTime time;
    double utcOffset = 0.0;
    QString timeUnit;

    // Try to parse the attribute string according to the time units pattern
    // (sec/min/hours/...) since YYYY-MM-DD HH ...).
    bool success = parseTimeUnits(QString::fromStdString(att), &time,
                                  &utcOffset, &timeUnit);
    if (success) return time;

    // If the first parse attempt fails, add an "hours since" to the attribute
    // and try again.
    string att2 = "hours since " + att;
    success = parseTimeUnits(QString::fromStdString(att2), &time,
                             &utcOffset, &timeUnit);
    if (success) return time;

    // We can't interpret this time string.
    throw NcException(
                "NcException",
                "cannot identify time attribute "+att,
                __FILE__, __LINE__);
}


NcCFVar::NcVariableGridType NcCFVar::getGridType()
{
    if (varGridType != UNDEFINED) return varGridType;

    // Only continue with checks if the variable type is UNDEFINED (i.e. has not
    // yet been defined).
    if      (isCFDataVariable(*this, LAT_LON_PVU))    varGridType = LAT_LON_PVU;
    else if (isCFDataVariable(*this, LAT_LON_HYBRID)) varGridType = LAT_LON_HYBRID;
    else if (isCFDataVariable(*this, LAT_LON_P))      varGridType = LAT_LON_P;
    else if (isCFDataVariable(*this, LAT_LON))        varGridType = LAT_LON;
    return varGridType;
}


bool NcCFVar::isCFDataVariable(const NcVar& var, NcVariableGridType type)
{
    // First test: If the variable has only one dimension and that dimension
    // has the same name as the variable, it is a coordinate variable.
    if ((var.getDimCount() == 1) && (var.getName() == var.getDim(0).getName()))
        return false;

#ifdef NETCDF_CF_TEST_ATTRIBUTES
    // Second test: The variable has to contain the "units" attribute and
    // either the "long_name" or "standard_name" attribute.
    bool units=false, name=false;
    map<string,NcVarAtt> attributeList = var.getAtts();
    if (attributeList.find("units") != attributeList.end()) units = true;
    if (attributeList.find("long_name") != attributeList.end()) name = true;
    if (attributeList.find("standard_name") != attributeList.end()) name = true;
    if (!(units && name)) return false;
#endif

    // Third test: Are all coordinates for the requested variable type present?
    NcCFVar cfvar(var);
    switch (type) {
    case UNDEFINED:
    case ALL:
        break;
    case LAT_LON:
        try { NcVar dummy = cfvar.getLongitudeVar(); }
        catch (NcException) { return false; }
        try { NcVar dummy = cfvar.getLatitudeVar(); }
        catch (NcException) { return false; }
        break;
    case LAT_LON_P:
        try { NcVar dummy = cfvar.getVerticalCoordinatePressure(); }
        catch (NcException) { return false; }
        try { NcVar dummy = cfvar.getLongitudeVar(); }
        catch (NcException) { return false; }
        try { NcVar dummy = cfvar.getLatitudeVar(); }
        catch (NcException) { return false; }
        break;
    case LAT_LON_HYBRID:
        try { NcVar a, b; QString ps; NcVar dummy =
                cfvar.getVerticalCoordinateHybridSigmaPressure(&a, &b, &ps); }
        catch (NcException) { return false; }
        try { NcVar dummy = cfvar.getLongitudeVar(); }
        catch (NcException) { return false; }
        try { NcVar dummy = cfvar.getLatitudeVar(); }
        catch (NcException) { return false; }
        break;
    case LAT_LON_PVU:
        try { NcVar dummy = cfvar.getVerticalCoordinatePotVort(); }
        catch (NcException) { return false; }
        try { NcVar dummy = cfvar.getLongitudeVar(); }
        catch (NcException) { return false; }
        try { NcVar dummy = cfvar.getLatitudeVar(); }
        catch (NcException) { return false; }
        break;
    }

    // All tests have been passed: The variable is a CF data variable!
    return true;
}


/******************************************************************************
***                           PRIVATE METHODS                               ***
*******************************************************************************/

bool NcCFVar::parseTimeUnits(QString units, QDateTime *baseTime,
                             double *utcOffset, QString *timeUnit)
{
    // Should the functionality provided by this function become insufficient,
    // (a) the udunits2 package is capable of parsing time units (not well
    // documented, grep for "since " in the source code), and (b) there seems
    // to be some extensive calender package implemented in NCO, source code
    // files nco_cln_utl.h and nco_cln_utl.c. The latter also supports
    // calenders other than the combined Julian/Gregorian supported by udunits
    // and QDateTime. -- (mr, 13Sept2011).

    // If the given units string cannot be matched to the regular expression,
    // return false..
    if (reTimeUnits.indexIn(units) < 0) return false;

    // ..otherwise the regular expression has been matched. The capturedTexts()
    // method returns the values of the regexp groups that have been matched
    // (containing the data we need to recover the base time).
    QStringList parsedValues = reTimeUnits.capturedTexts();

    // First group: unit of the time axis. Convert abbreviations to the full
    // name. (Cf.
    // http://cf-pcmdi.llnl.gov/documents/cf-conventions/1.5/cf-conventions.html#time-coordinate).
    // Note that a possibly present plural "s" (e.g. minute"s") is removed
    // by the regular expression parser.
    *timeUnit = parsedValues.at(1).toLower();
    if      ((*timeUnit == "sec") || (*timeUnit == "s")) *timeUnit = "second";
    else if (*timeUnit == "min")                         *timeUnit = "minute";
    else if ((*timeUnit == "hr") || (*timeUnit == "h"))  *timeUnit = "hour";
    else if (*timeUnit == "d")                           *timeUnit = "day";
    else if (*timeUnit == "yr")                          *timeUnit = "year";

    // Groups 2-4 contain the date, 5-7 the time. Error checking is omitted as
    // the regular expression only matches number literals to these groups. If
    // no time has been specified in the units string, toInt() and toDouble()
    // return 0, as would be the default time anyway.
    bool ok;
    baseTime->setDate(QDate(parsedValues.at(2).toInt(&ok),   // year
                            parsedValues.at(3).toInt(&ok),   // month
                            parsedValues.at(4).toInt(&ok))); // day

    baseTime->setTime(QTime(parsedValues.at(5).toInt(&ok),      // hour
                            parsedValues.at(6).toInt(&ok),      // minute
                            parsedValues.at(7).toDouble(&ok))); // second, can be float

    baseTime->setTimeSpec(Qt::UTC);

    // Groups 8 and 9, if given, contain the time zone offset to UTC. If these
    // groups are not matched, utcOffset will be 0.
    *utcOffset = parsedValues.at(8).toInt(&ok) + 60 * parsedValues.at(9).toInt(&ok);

    return true;
}
