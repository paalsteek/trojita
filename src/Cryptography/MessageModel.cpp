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

#include "MessagePartFactory.h"
#include "MessageModel.h"
#include "Imap/Encoders.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include <QtGui/QFont>

namespace Cryptography {

MessagePart::MessagePart(MessagePart *parent)
    : m_parent(parent)
    , m_children()
    , m_rawPart(nullptr)
    , m_row(0)
{
}

MessagePart::~MessagePart()
{
    Q_FOREACH(MessagePart* part, m_children) {
        delete part;
    }
    if (m_rawPart)
        delete m_rawPart;
}

MessagePart* MessagePart::parent() const
{
    return m_parent;
}

const int MessagePart::row() const
{
    return m_row;
}

int MessagePart::rowCount() const
{
    return m_children.size();
}

MessagePart* MessagePart::child(int row, int column) const
{
    if ( column == Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS ) {
        Q_ASSERT(row == 0);
        return m_rawPart;
    }

    Q_ASSERT(column == 0);
    if ( m_children.size() <= row ) {
        return nullptr;
    }
    return m_children[row];
}

void MessagePart::setChild(int row, int column, MessagePart *part)
{
    Q_UNUSED(column);
    Q_ASSERT(m_children.size() >= row);
    m_children.insert(row, part);
    part->setParent(this);
    part->setRow(row);
}

void MessagePart::setRow(int row)
{
    m_row = row;
}

void MessagePart::setParent(MessagePart *parent)
{
    m_parent = parent;
}

void MessagePart::setRawPart(MessagePart *rawPart)
{
    Q_ASSERT(!m_rawPart);
    m_rawPart = rawPart;
}

ProxyMessagePart::ProxyMessagePart(MessagePart *parent, const QModelIndex& sourceIndex, MessageModel* model)
    : MessagePart(parent)
    , m_sourceIndex(sourceIndex)
{
    model->addIndexMapping(sourceIndex, this);
}

ProxyMessagePart::~ProxyMessagePart()
{
}

QVariant ProxyMessagePart::data(int role) const
{
    return m_sourceIndex.data(role);
}

LocalMessagePart::LocalMessagePart(MessagePart *parent, const QByteArray& mimetype)
    : MessagePart(parent)
    , m_envelope(0)
    , m_state(NONE)
    , m_mimetype(mimetype)
{
}

LocalMessagePart::~LocalMessagePart()
{
}

void LocalMessagePart::setData(const QByteArray &data)
{
    m_data = data;
    m_state = DONE;
}

void LocalMessagePart::setFetchingState(FetchingState state)
{
    m_state = state;
}

LocalMessagePart::FetchingState LocalMessagePart::getFetchingState() const
{
    return m_state;
}

void LocalMessagePart::setCharset(const QString &charset)
{
    m_charset = charset;
}

void LocalMessagePart::setContentFormat(const QString &format)
{
    m_contentFormat = format;
}

void LocalMessagePart::setDelSp(const QString &delSp)
{
    m_delSp = delSp;
}

void LocalMessagePart::setFilename(const QString &filename)
{
    m_filename = filename;
}

void LocalMessagePart::setEncoding(const QByteArray &encoding)
{
    m_encoding = encoding;
}

void LocalMessagePart::setBodyFldId(const QByteArray &bodyFldId)
{
    m_bodyFldId = bodyFldId;
}

void LocalMessagePart::setBodyDisposition(const QByteArray &bodyDisposition)
{
    m_bodyDisposition = bodyDisposition;
}

void LocalMessagePart::setMultipartRelatedStartPart(const QByteArray &startPart)
{
    m_multipartRelatedStartPart = startPart;
}

void LocalMessagePart::setOctets(int octets)
{
    m_octets = octets;
}

void LocalMessagePart::setEnvelope(Imap::Message::Envelope *envelope)
{
    m_envelope = envelope;
}

bool LocalMessagePart::isTopLevelMultipart() const
{
    return m_mimetype.startsWith("multipart/") && (!parent()->parent() || parent()->data(Imap::Mailbox::RolePartMimeType).toString().startsWith("message/"));
}

QString LocalMessagePart::partId() const
{
    if (isTopLevelMultipart())
        return QString();

    QString id = QString::number(row() + 1);
    if (parent() && !parent()->data(Imap::Mailbox::RolePartId).toString().isEmpty())
        id = parent()->data(Imap::Mailbox::RolePartId).toString() + QChar('.') + id;

    return id;
}

QString LocalMessagePart::pathToPart() const {
    // TODO: do we need some way to prevent fetching of local parts?
    QString parentPath = QLatin1String("");
    if (parent()) {
        parentPath = parent()->data(Imap::Mailbox::RolePartPathToPart).toString();
    }
    return parentPath + QLatin1Char('/') + QString::number(row());
}

QVariant LocalMessagePart::data(int role) const
{
    if (role == Imap::Mailbox::RoleIsFetched) {
        return m_state == DONE;
    }

    switch (role) {
    case Qt::DisplayRole:
        if (isTopLevelMultipart())
            return QString("%1").arg(QString::fromLocal8Bit(m_mimetype));
        else
            return QString("%1: %2").arg(partId(), QString::fromLocal8Bit(m_mimetype));
    case Qt::ToolTipRole:
    {
        return m_octets > 10000 ? QString("%1 bytes of data").arg(m_octets) : m_data;
    }
    case Qt::FontRole:
    {
        QFont f;
        f.setItalic(true);
        return f;
    }
    }

    if (m_mimetype.toLower() == QByteArray("message/rfc822")) {
        switch (role) {
        case Imap::Mailbox::RoleMessageEnvelope:
            if (m_envelope)
                return QVariant::fromValue<Imap::Message::Envelope>(*m_envelope);
            else
                return QVariant();
        }
    }

    switch (role) {
    case Imap::Mailbox::RoleIsUnavailable:
        return m_state == UNAVAILABLE;
    case Imap::Mailbox::RolePartData:
        return m_data;
    case Imap::Mailbox::RolePartUnicodeText:
    {
        if (m_mimetype.startsWith("text/")) {
            return Imap::decodeByteArray(m_data, m_charset.toLocal8Bit());
        } else {
            return QVariant();
        }
    }
    case Imap::Mailbox::RolePartMimeType:
        return m_mimetype;
    case Imap::Mailbox::RolePartCharset:
        return m_charset;
    case Imap::Mailbox::RolePartContentFormat:
        return m_contentFormat;
    case Imap::Mailbox::RolePartContentDelSp:
        return m_delSp;
    case Imap::Mailbox::RolePartEncoding:
        return m_encoding;
    case Imap::Mailbox::RolePartBodyFldId:
        return m_bodyFldId;
    case Imap::Mailbox::RolePartBodyDisposition:
        return m_bodyDisposition;
    case Imap::Mailbox::RolePartFileName:
        return m_filename;
    case Imap::Mailbox::RolePartOctets:
        return m_octets;
    case Imap::Mailbox::RolePartId:
        return partId();
    case Imap::Mailbox::RolePartPathToPart:
        return pathToPart();
    case Imap::Mailbox::RolePartMultipartRelatedMainCid:
        if (m_multipartRelatedStartPart.isEmpty()) {
            return m_multipartRelatedStartPart;
        } else {
            return QVariant();
        }
    case Imap::Mailbox::RolePartIsTopLevelMultipart:
        return isTopLevelMultipart();
    case Imap::Mailbox::RolePartForceFetchFromCache:
        return QVariant(); // Nothing to do here
    default:
        break;
    }

    return QVariant();
}

MessageModel::MessageModel(QObject *parent, const QModelIndex &message)
    : QAbstractItemModel(parent)
    , m_message(message)
    , m_rootPart(0)
    , m_factory(new MessagePartFactory(this))
{
    connect(message.model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(mapDataChanged(QModelIndex,QModelIndex)));
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
    if (localPart && localPart->getFetchingState() == LocalMessagePart::NONE) {
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

    if ( role == Imap::Mailbox::RoleMessageUid
         || role == Imap::Mailbox::RoleMailboxName
         || role == Imap::Mailbox::RoleMailboxUidValidity ) {
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
        LocalMessagePart *localPart = dynamic_cast<LocalMessagePart*>(part);
        if (localPart)
            localPart->setFetchingState(LocalMessagePart::DONE);
    } else {
        Q_ASSERT(!m_rootPart);
        m_rootPart = children.first();
    }
    endInsertRows();
}
}
