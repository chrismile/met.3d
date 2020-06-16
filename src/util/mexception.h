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
#ifndef MEXCEPTION_H
#define MEXCEPTION_H

#include <exception>
#include <string>

using namespace std;

namespace Met3D
{

/**
  Base class for all exceptions occuring in Met.3D.
  */
class MException : public std::exception
{
public:
    MException(const std::string& exceptionName, const std::string& complaint,
               const char* file, int line);
    virtual ~MException() throw();
    const char* what() const throw();

private:
    string message;
    string exceptionName;
    string fileName;
    int lineNumber;
};


/**
  Thrown if an invalid data field is requested in the data pipeline.
  */
class MBadDataFieldRequest : public MException
{
public:
    MBadDataFieldRequest(const std::string& complaint,
                         const char* file, int line);
};


/**
  Thrown if a class in the Met.3D framework could not be correctly initialised.
  */
class MInitialisationError : public MException
{
public:
    MInitialisationError(const std::string& complaint,
                         const char* file, int line);
};


/**
  Invalid keys have been requested (e.g. data source id, variable name, ...).
  */
class MKeyError : public MException
{
public:
    MKeyError(const std::string& complaint,
              const char* file, int line);
};


/**
  Invalid values have been specified (e.g. a constant, ...).
  */
class MValueError : public MException
{
public:
    MValueError(const std::string& complaint,
                const char* file, int line);
};


/**
  Invalid values have been specified (e.g. a constant, ...).
  */
class MMemoryError : public MException
{
public:
    MMemoryError(const std::string& complaint,
                 const char* file, int line);
};


/**
  Something goes wrong when accessing a Grib file.
  */
class MGribError : public MException
{
public:
    MGribError(const std::string& complaint,
               const char* file, int line);
};

} // namespace Met3D

#endif // MEXCEPTION_H
