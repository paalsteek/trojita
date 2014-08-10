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

#include <QtGui/QFont>

#include "MessagePart.h"

#include "MessageModel.h"
#include "Imap/Encoders.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/ItemRoles.h"
#include "Common/MetaTypes.h"

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
    if (column == Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS) {
        Q_ASSERT(row == 0);
        return m_rawPart.get();
    }

    Q_ASSERT(column == 0);
    if (m_children.size() <= row) {
        return nullptr;
    }
    return m_children[row];
}

void MessagePart::setChild(int row, int column, MessagePart *part)
{
    Q_UNUSED(column);
    Q_ASSERT(m_children.size() >= row);
    Q_ASSERT(part);
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
    m_rawPart = std::unique_ptr<MessagePart>(rawPart);
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

LocalMessagePart::FetchingState LocalMessagePart::fetchingState() const
{
    return m_state;
}

void LocalMessagePart::setCharset(const QByteArray &charset)
{
    m_charset = charset;
}

void LocalMessagePart::setContentFormat(const QByteArray &format)
{
    m_contentFormat = format;
}

void LocalMessagePart::setDelSp(const QByteArray &delSp)
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

void LocalMessagePart::setOctets(uint octets)
{
    m_octets = octets;
}

void LocalMessagePart::setEnvelope(Imap::Message::Envelope *envelope)
{
    m_envelope = envelope;
}

void LocalMessagePart::setHdrReferences(const QList<QByteArray> &references)
{
    m_hdrReferences = references;
}

void LocalMessagePart::setHdrListPost(const QList<QUrl> &listPost)
{
    m_hdrListPost = listPost;
}

void LocalMessagePart::setHdrListPostNo(const bool listPostNo)
{
    m_hdrListPostNo = listPostNo;
}

bool LocalMessagePart::isTopLevelMultipart() const
{
    return m_mimetype.startsWith("multipart/") && (!parent()->parent() || parent()->data(Imap::Mailbox::RolePartMimeType).toByteArray().startsWith("message/"));
}

QByteArray LocalMessagePart::partId() const
{
    if (isTopLevelMultipart())
        return QByteArray();

    QByteArray id = QByteArray::number(row() + 1);
    if (parent() && !parent()->data(Imap::Mailbox::RolePartId).toByteArray().isEmpty())
        id = parent()->data(Imap::Mailbox::RolePartId).toByteArray() + '.' + id;

    return id;
}

QByteArray LocalMessagePart::pathToPart() const {
    QByteArray parentPath;
    if (parent()) {
        parentPath = parent()->data(Imap::Mailbox::RolePartPathToPart).toByteArray();
    }

    return parentPath + '/' + QByteArray::number(row());
}

QVariant LocalMessagePart::data(int role) const
{
    if (role == Imap::Mailbox::RoleIsFetched) {
        return m_state == DONE;
    }

    // This set of roles is for debugging purposes only
    switch (role) {
    case Qt::DisplayRole:
        if (isTopLevelMultipart())
            return QString::fromUtf8("%1").arg(QString::fromUtf8(m_mimetype));
        else
            return QString::fromUtf8("%1: %2").arg(QString::fromUtf8(partId()), QString::fromUtf8(m_mimetype));
    case Qt::ToolTipRole:
    {
        return m_octets > 10000 ? QString::fromUtf8("%1 bytes of data").arg(m_octets) : QString::fromUtf8(m_data);
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
        case Imap::Mailbox::RoleMessageHeaderReferences:
            return QVariant::fromValue(m_hdrReferences);
        case Imap::Mailbox::RoleMessageHeaderListPost:
        {
            QVariantList res;
            Q_FOREACH(const QUrl &url, m_hdrListPost)
                res << url;
            return res;
        }
        case Imap::Mailbox::RoleMessageHeaderListPostNo:
            return m_hdrListPostNo;
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
            return Imap::decodeByteArray(m_data, m_charset);
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
    case Imap::Mailbox::RolePartBufferPtr:
        return QVariant::fromValue(const_cast<QByteArray*>(&m_data));
    default:
        break;
    }

    return QVariant();
}
}
