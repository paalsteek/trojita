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

#include "MessageModel.h"
#include "MessagePart.h"
#include "MessagePartFactory.h"
#include "Imap/Model/ItemRoles.h"
#include "Common/MetaTypes.h"

namespace Cryptography {

MessageModel::MessageModel(QObject *parent, const QModelIndex &message)
    : QAbstractItemModel(parent)
    , m_message(message)
    , m_rootPart(0)
    , m_factory(new MessagePartFactory(this))
{
    // Make sure the message structure is loaded
    Q_ASSERT(m_message.model()->rowCount(m_message) > 0);

    connect(m_message.model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(mapDataChanged(QModelIndex,QModelIndex)));
    m_factory->buildSubtree(message);
}

void MessageModel::mapDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    QModelIndex root = index(0,0);
    if (!root.isValid())
        return;

    Q_ASSERT(topLeft.parent() == bottomRight.parent());
    if (m_map.contains(topLeft)) {
        emit dataChanged(
                    createIndex(topLeft.row(), topLeft.column(), m_map[topLeft]),
                    createIndex(bottomRight.row(), bottomRight.column(), m_map[bottomRight])
                    );
    }
}

MessageModel::~MessageModel()
{
}

QModelIndex MessageModel::index(int row, int column, const QModelIndex &parent) const
{
    MessagePart* child;
    if (!parent.isValid()) {
        if (!m_rootPart) {
            return QModelIndex();
        }
        child = m_rootPart;
    } else {
        MessagePart* part = static_cast<MessagePart*>(parent.internalPointer());
        Q_ASSERT(part);
        child = part->child(row, column);
    }

    if (!child) {
        return QModelIndex();
    }

    return createIndex(row, column, child);
}

QModelIndex MessageModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();

    MessagePart* part = static_cast<MessagePart*>(child.internalPointer());
    Q_ASSERT(part);
    if (!part->parent())
        return QModelIndex();

    return createIndex(part->row(), 0, part->parent());

    return QModelIndex();
}

int MessageModel::rowCount(const QModelIndex &parent) const
{
    if (!parent.isValid())
        return !m_rootPart ? 0 : 1;

    MessagePart* part = static_cast<MessagePart*>(parent.internalPointer());
    Q_ASSERT(part);
    int count = part->rowCount();
    LocalMessagePart *localPart = dynamic_cast<LocalMessagePart*>(part);
    if (localPart && localPart->fetchingState() == LocalMessagePart::NONE) {
        localPart->setFetchingState(LocalMessagePart::LOADING);
        m_factory->buildSubtree(parent);
    }

    return count;
}

int MessageModel::columnCount(const QModelIndex &parent) const
{
    Q_UNUSED(parent);
    return !m_rootPart ? 0 : 1;
}

QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(index.isValid());

    switch (role) {
    case Imap::Mailbox::RolePartMessageIndex:
        return QVariant::fromValue(message());
    case Imap::Mailbox::RoleMessageUid:
    case Imap::Mailbox::RoleMailboxName:
    case Imap::Mailbox::RoleMailboxUidValidity:
        return message().data(role);
    }

    MessagePart* part = static_cast<MessagePart*>(index.internalPointer());
    Q_ASSERT(part);

    return part->data(role);
}

QModelIndex MessageModel::message() const {
     return m_message;
}

void MessageModel::addIndexMapping(QModelIndex source, MessagePart *destination)
{
    m_map.insert(source, destination);
}

void MessageModel::insertSubtree(const QModelIndex& parent, const QVector<Cryptography::MessagePart *> &children)
{
    beginInsertRows(parent, 0, children.size());
    if (parent.isValid()) {
        Q_ASSERT(rowCount(parent) == 0);
        MessagePart* part = static_cast<MessagePart*>(parent.internalPointer());
        Q_ASSERT(part);
        for (int i = 0; i < children.size(); ++i) {
            part->setChild(i, 0, children[i]);
        }
        if (LocalMessagePart *localPart = dynamic_cast<LocalMessagePart*>(part)) {
            localPart->setFetchingState(LocalMessagePart::DONE);
        }
    } else {
        Q_ASSERT(!m_rootPart);
        m_rootPart = children.first();
    }
    endInsertRows();
}
}
