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
#include "datarequest.h"

// standard library imports

// related third party imports

// local application imports

using namespace std;

namespace Met3D
{

/******************************************************************************
***                     CONSTRUCTOR / DESTRUCTOR                            ***
*******************************************************************************/

MDataRequestHelper::MDataRequestHelper()
    : modified(true)
{
}


MDataRequestHelper::MDataRequestHelper(const MDataRequest &request)
    : modified(true)
{
    QStringList requestList = request.split(";", QString::SkipEmptyParts);

    QStringListIterator it(requestList);
    while (it.hasNext())
    {
        QString keyValuePair = it.next();
        QString key   = keyValuePair.section("=", 0, 0);
        QString value = keyValuePair.section("=", 1, 1);
        requestMap.insert(key, value);
    }
}


MDataRequestHelper::~MDataRequestHelper()
{
}


/******************************************************************************
***                            PUBLIC METHODS                               ***
*******************************************************************************/

void MDataRequestHelper::unite(const MDataRequestHelper &other)
{
    requestMap = requestMap.unite(other.map());
}


void MDataRequestHelper::addKeyPrefix(QString prefix)
{
    QList<QString> keys = requestMap.keys();
    for (int i = 0; i < keys.size(); i++)
        requestMap.insert(prefix + keys[i], requestMap.take(keys[i]));
}


void MDataRequestHelper::removeKeyPrefix(QString prefix)
{
    QList<QString> keys = requestMap.keys();
    for (int i = 0; i < keys.size(); i++)
        if (keys[i].startsWith(prefix))
            requestMap.insert(keys[i].right(keys[i].size()-prefix.size()),
                              requestMap.take(keys[i]));
}


MDataRequestHelper MDataRequestHelper::subRequest(QString prefix)
{
    MDataRequestHelper rh;

    QList<QString> keys = requestMap.keys();
    for (int i = 0; i < keys.size(); i++)
        if (keys[i].startsWith(prefix))
            rh.insert(keys[i].right(keys[i].size()-prefix.size()),
                      requestMap.value(keys[i]));

    return rh;
}


bool MDataRequestHelper::contains(const QString &key) const
{
    return requestMap.contains(key);
}


bool MDataRequestHelper::containsAll(const QList<QString> &keys) const
{
    for (int i = 0; i < keys.size(); i++)
        if (!requestMap.contains(keys.at(i))) return false;
    return true;
}


float MDataRequestHelper::floatValue(const QString &key) const
{
    return requestMap.value(key, QString()).toFloat();
}


int MDataRequestHelper::intValue(const QString &key) const
{
    return requestMap.value(key, QString()).toInt();
}


void MDataRequestHelper::insert(const QString &key, const QString &value)
{
    modified = true;
    requestMap.insert(key, value);
}


void MDataRequestHelper::insert(const QString &key, const int &value)
{
    modified = true;
    requestMap.insert(key, QString("%1").arg(value));
}


void MDataRequestHelper::insert(const QString &key, const QDateTime &value)
{
    modified = true;
    requestMap.insert(key, value.toString(Qt::ISODate));
}


void MDataRequestHelper::insert(const QString &key, const QVector3D &value)
{
    modified = true;
    QString s = QString("%1/%2/%3").arg(value.x()).arg(value.y()).arg(value.z());
    requestMap.insert(key, s);
}


QString MDataRequestHelper::uintSetToString(const QSet<unsigned int> &value)
{
    QString s;

    if (value.count() > 0)
    {
        // QSet items are unsorted. In the request, they need to be sorted to
        // ensure that there are no requests that reference the same members
        // but in a different order (the memory manager can't distinguish).
        QList<unsigned int> list = value.toList();
        qSort(list);

//TODO (mr, 26Nov2015) -- convert the list of members to a more compact form,
//                        e.g. 0/5:10/20:22/25 for 0/5/6/7/8/9/10/20/21/22/25.

        foreach (unsigned int i, list) s += QString("%1/").arg(i);
        s = s.left(s.length() - 1); // remove last "/"
    }

    return s;
}


void MDataRequestHelper::insert(const QString &key, const QSet<unsigned int> &value)
{
    modified = true;

    if (value.count() > 0)
    {
        requestMap.insert(key, uintSetToString(value));
    }
}


QStringList MDataRequestHelper::keys() const
{
    return requestMap.keys();
}


const QMap<QString, QString> &MDataRequestHelper::map() const
{
    return requestMap;
}


bool MDataRequestHelper::remove(const QString &key)
{
    modified = true;
    return bool(requestMap.remove(key));
}


void MDataRequestHelper::removeAllKeysExcept(const QStringList &keepTheseKeys)
{
    modified = true;

    QList<QString> keys = requestMap.keys();
    for (int i = 0; i < keys.size(); i++)
    {
        QString key = keys[i];
        if (!keepTheseKeys.contains(key)) requestMap.remove(key);
    }
}


void MDataRequestHelper::removeAll(const QStringList &keys)
{
    modified = true;
    for (int i = 0; i < keys.size(); i++) requestMap.remove(keys[i]);
}


MDataRequest MDataRequestHelper::request()
{
    if (modified) updateRequestString();
    return requestString;
}


QDateTime MDataRequestHelper::timeValue(const QString &key) const
{
    return QDateTime::fromString(requestMap.value(key, QString()), Qt::ISODate);
}


QString MDataRequestHelper::value(const QString &key) const
{
    return requestMap.value(key, QString());
}


QVector3D MDataRequestHelper::vec3Value(const QString &key) const
{
    QString s = value(key);
    if (s.isEmpty()) return QVector3D();

    QStringList sl = s.split("/");
    if (sl.size() < 3) return QVector3D();
    return QVector3D(sl[0].toFloat(), sl[1].toFloat(), sl[2].toFloat());
}


QSet<unsigned int> MDataRequestHelper::uintSetValue(const QString &key) const
{
    QString s = value(key);
    QSet<unsigned int> set;
    QStringList sl = s.split("/");

    foreach (QString slItem, sl)
    {
        bool ok = false;
        unsigned int i = slItem.toUInt(&ok);
        if (ok) set.insert(i);
    }

    return set;
}


/******************************************************************************
***                            PRIVATE METHODS                              ***
*******************************************************************************/

void MDataRequestHelper::updateRequestString()
{
    requestString = "";

    QMapIterator<QString, QString> i(requestMap);
    while (i.hasNext())
    {
        i.next();
        requestString += QString("%1=%2;").arg(i.key()).arg(i.value());
    }

    modified = false;
}

} // namespace Met3D
