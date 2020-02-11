/******************************************************************************
**
**  This file is part of Met.3D -- a research environment for the
**  three-dimensional visual exploration of numerical ensemble weather
**  prediction data.
**
**  Copyright 2014 Marc Rautenhaus
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
#include "mexception.h"
#include <iostream>
#include <sstream>

namespace Met3D
{

MException::MException(const std::string& exceptionName, const std::string& complaint,
                       const char* file, int line)
    : message(complaint),
      exceptionName(exceptionName),
      fileName(file),
      lineNumber(line)
{
    if (complaint.length() == 0) message = "A Met.3D exception has occured";
}


MException::~MException() throw()
{
}


const char* MException::what() const throw()
{
    stringstream s;
    s << "in " << fileName << " (line " << lineNumber << "), exception "
      << exceptionName << " has been thrown: " << message << "\n\n";
    return s.str().c_str();
}


MBadDataFieldRequest::MBadDataFieldRequest(const std::string &complaint,
                                           const char *file, int line)
    : MException("MBadDataFieldRequest", complaint, file, line)
{
}


MInitialisationError::MInitialisationError(const std::string &complaint,
                                           const char *file, int line)
    : MException("MInitialisationError", complaint, file, line)
{
}


MKeyError::MKeyError(const std::string &complaint,
                     const char *file, int line)
    : MException("MKeyError", complaint, file, line)
{
}


MValueError::MValueError(const std::string &complaint,
                         const char *file, int line)
    : MException("MValueError", complaint, file, line)
{
}


MMemoryError::MMemoryError(const std::string &complaint,
                           const char *file, int line)
    : MException("MMemoryError", complaint, file, line)
{
}


MGribError::MGribError(const std::string &complaint,
                       const char *file, int line)
    : MException("MGribError", complaint, file, line)
{
}

} // namespace Met3D
