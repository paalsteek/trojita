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

#ifndef CRYPTOGRAPHY_MESSAGEMODEL_H
#define CRYPTOGRAPHY_MESSAGEMODEL_H

#include <QHash>
#include <QPersistentModelIndex>

namespace Cryptography {
class OpenPGPHelper;
class SMIMEHelper;
}

namespace Cryptography {
class MessagePart;
class MessagePartFactory;

/** @short A model for a single message including raw and possibly decrypted message parts */
class MessageModel: public QAbstractItemModel
{
    Q_OBJECT

public:
    MessageModel(QObject *parent, const QModelIndex& message);
    ~MessageModel();

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;

    QModelIndex message() const;

    /** @short Adds a mapping from a source QModelIndex to a MessagePart
     * to support handling of signals from the original model
     */
    void addIndexMapping(QModelIndex source, MessagePart* destination);

public slots:
    void insertSubtree(const QModelIndex& parent, const QVector<Cryptography::MessagePart*>& children);
    void mapDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);

protected:
    const QPersistentModelIndex m_message;
    QHash<QModelIndex, MessagePart*> m_map;
    MessagePart *m_rootPart;
    MessagePartFactory *m_factory;
};
}
#endif /* CRYPTOGRAPHY_MESSAGEMODEL_H */
