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
#include <QIdentityProxyModel>

#include "Cryptography/OpenPGPHelper.h"
#include "Cryptography/SMIMEHelper.h"

namespace Common {

class MessagePart {
public:
    MessagePart(QModelIndex sourceIndex, MessagePart *parent = 0);
    MessagePart(mimetic::MimeEntity* pMe, MessagePart *parent = 0);
    ~MessagePart();

    void addChild(MessagePart *child) { m_children.append(child); }
    MessagePart* child(int row) { return m_children[row]; }
    MessagePart* parent() { return m_parent; }
    int rowCount() { return m_children.size(); }

    int findRow(MessagePart *child);

    QVariant mimetype();

private:
    MessagePart *m_parent;
    QList<MessagePart*> m_children;
    mimetic::MimeEntity *m_me;
    QModelIndex m_sourceIndex;
};

class MessageModel: public QAbstractItemModel
{
    Q_OBJECT

public:
    MessageModel(QModelIndex message, QObject *parent = 0);
    ~MessageModel();

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const { return 0; }
    QVariant data(const QModelIndex &index, int role) const;

private:
    QModelIndex m_message;
    Cryptography::OpenPGPHelper* m_pgpHelper;
    Cryptography::SMIMEHelper* m_smimeHelper;
};
}
#endif /* COMMON_MODEL_H */
