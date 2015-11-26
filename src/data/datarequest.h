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
#ifndef DATAREQUEST_H
#define DATAREQUEST_H

// standard library imports

// related third party imports
#include <QtCore>
#include <QtGui>

// local application imports


namespace Met3D
{

/**
  Data requests are encoded in a string.
 */
typedef QString MDataRequest;


/**
  @brief MDataRequestHelper provides convenience methods to generate and to
  parse a request.
  */
class MDataRequestHelper
{
public:
    /**
      Creates an empty request.
     */
    MDataRequestHelper();

    /**
      Parses @p request.
     */
    MDataRequestHelper(const MDataRequest &request);

    virtual ~MDataRequestHelper();

    void unite(const MDataRequestHelper &other);

    void addKeyPrefix(QString prefix);

    void removeKeyPrefix(QString prefix);

    MDataRequestHelper subRequest(QString prefix);

    bool contains(const QString &key) const;

    bool containsAll(const QList<QString> &keys) const;

    float floatValue(const QString &key) const;

    int intValue(const QString &key) const;

    void insert(const QString &key, const QString &value);

    void insert(const QString &key, const int &value);

    void insert(const QString &key, const QDateTime &value);

    void insert(const QString &key, const QVector3D &value);

    void insert(const QString &key, const QSet<unsigned int> &value);

    QStringList keys() const;

    const QMap<QString, QString>& map() const;

    bool remove(const QString &key);

    void removeAllKeysExcept(const QStringList &keepTheseKeys);

    void removeAll(const QStringList &keys);

    MDataRequest request();

    QDateTime timeValue(const QString &key) const;

    QString value(const QString &key) const;

    QVector3D vec3Value(const QString &key) const;

    QSet<unsigned int> uintSetValue(const QString &key) const;

private:
    /** Map that stores all key/value pairs of the request. QMap is used as
        this class returns all key/value pairs sorted according to the key
        (in contrast to QHash, which returns the items in arbitrary order. */
    QMap<QString, QString> requestMap;

    /** String that encodes the key/value pairs similar to a WMS request. */
    QString requestString;
    bool modified;

    /**
      Regenerates the request string from the @ref requestMap.
     */
    void updateRequestString();
};

} // namespace Met3D

#endif // DATAREQUEST_H
