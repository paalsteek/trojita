/* Copyright (C) 2014 Stephan Platz <trojita@paalsteek.de>

   This file is part of the Trojita Qt IMAP e-mail client,
   http://trojita.flaska.net/

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation; either version 2 of
   the License or (at your option) version 3 or any later version
   accepted by the membership of KDE e.V. (or its successor approved
   by the membership of KDE e.V.), which shall act as a proxy
   defined in Section 14 of version 3 of the license.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef COMMON_MODEL_H
#define COMMON_MODEL_H

#include <QHash>
#include <QObject>
#include <QPersistentModelIndex>

#include "configure.cmake.h"

#ifdef TROJITA_HAVE_MIMETIC
#include <mimetic/mimetic.h>
#endif /* TROJITA_HAVE_MIMETIC */

namespace Cryptography {
class OpenPGPHelper;
class SMIMEHelper;
}

namespace Common {
class MessagePartFactory;

class MessagePart
{
public:
    MessagePart(MessagePart *parent, int row);
    virtual ~MessagePart();

    MessagePart* parent() const { return m_parent; }
    const int row() const { return m_row; }
    MessagePart* child(int row, int column) const; //TODO: handle and use column
    int rowCount() const { return m_children.size(); }

    virtual QVariant data(int role) const = 0;

    void setChild(int row, int column, MessagePart* part);
    void setRow(int row) { m_row = row; }
    void setParent(MessagePart *parent) { m_parent = parent; }

private:
    MessagePart *m_parent;
    QVector<MessagePart*> m_children;
    int m_row;
};

class ProxyMessagePart : public MessagePart
{
public:
    ProxyMessagePart(MessagePart *parent, int row, const QModelIndex &sourceIndex);
    ~ProxyMessagePart();

    QVariant data(int role) const { return m_sourceIndex.data(role); }

private:
    QPersistentModelIndex m_sourceIndex;
};

class LocalMessagePart : public MessagePart
{
public:
    LocalMessagePart(MessagePart *parent, int row);
    ~LocalMessagePart();

    QVariant data(int role) const;

    void setData(const QByteArray& data) { m_data = data; }
    void setRawChild(MessagePart *rawChild);

protected:
    QByteArray m_data;
    MessagePart* m_rawChild;
};

class MessageModel: public QAbstractItemModel
{
    Q_OBJECT

public:
    MessageModel(QObject *parent, const QModelIndex& message);
    ~MessageModel();

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const { return !m_rootPart ? 0 : 1; }
    QVariant data(const QModelIndex &index, int role) const;
    void insertSubtree(const QModelIndex& parent, int row, int column, const QVector<MessagePart*>& children);

    QModelIndex message() { return m_message; }

protected:
    const QPersistentModelIndex m_message;
    MessagePart *m_rootPart;
    MessagePartFactory *m_factory;
};
}
#endif /* COMMON_MODEL_H */
