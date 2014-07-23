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
#include "ProxyModel.h"
#include "Imap/Encoders.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Network/MsgPartNetAccessManager.h" //TODO: move pathToPart to a public location
#include "Imap/Parser/Message.h"
#include <QDebug>
#include <QDateTime>

#include "Cryptography/OpenPGPHelper.h"
#include "Cryptography/SMIMEHelper.h"

#ifdef TROJITA_HAVE_QCA
#include <QtCrypto/QtCrypto>
#endif /* TROJITA_HAVE_QCA */

namespace Common {

#ifdef TROJITA_HAVE_QCA
QCA::Initializer init;
#endif /* TROJITA_HAVE_QCA */

//TODO: handling of special columns

MessagePart::MessagePart(MessagePart *parent)
    : m_parent(parent)
    , m_children()
    , m_rawPart(nullptr)
    , m_row(0)
{
}

MessagePart::~MessagePart()
{
    // TODO: cleanup children/rawPart
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

void MessagePart::setRawPart(MessagePart *rawPart)
{
    Q_ASSERT(!m_rawPart);
    m_rawPart = rawPart;
}

ProxyMessagePart::ProxyMessagePart(MessagePart *parent, const QModelIndex& sourceIndex)
    : MessagePart(parent)
    , m_sourceIndex(sourceIndex)
{
}

ProxyMessagePart::~ProxyMessagePart()
{
}

LocalMessagePart::LocalMessagePart(MessagePart *parent, const QByteArray& mimetype)
    : MessagePart(parent)
    , m_state(NONE)
    , m_mimetype(mimetype)
{
}

LocalMessagePart::~LocalMessagePart()
{
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
    // This item is not directly fetcheable, so it does *not* make sense to ask for it.
    // We cannot really assert at this point, though, because this function is published via the MVC interface.
    //return QLatin1String("application-bug-dont-fetch-this");
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
    }

    /*if (mimetype().toLower() == QLatin1String("message/rfc822")) {
        switch (role) {
        case Imap::Mailbox::RoleMessageEnvelope:
            mimetic::Header h = (*(m_me->body().parts().begin()))->header();
            QDateTime date = QDateTime::fromString(QString::fromStdString(h.field("date").value()));
            QString subject = QString::fromStdString(h.subject());
            QList<Imap::Message::MailAddress> from, sender, replyTo, to, cc, bcc;
            from = mailboxListToQList(h.from());
            if (h.hasField("sender")) {
                sender.append(Imap::Message::MailAddress(
                                  QString::fromStdString(h.sender().label()),
                                  QString::fromStdString(h.sender().sourceroute()),
                                  QString::fromStdString(h.sender().mailbox()),
                                  QString::fromStdString(h.sender().domain())));
            }
            replyTo = addressListToQList(h.replyto());
            to = addressListToQList(h.to());
            cc = addressListToQList(h.cc());
            bcc = addressListToQList(h.bcc());
            QList<QByteArray> inReplyTo;
            if (h.hasField("In-Reply-To")) {
                inReplyTo.append(QByteArray(h.field("In-Reply-To").value().c_str(), h.field("In-Reply-To").value().length()));
            }
            QByteArray messageId = QByteArray::fromRawData(h.messageid().str().c_str(), h.messageid().str().length());
            return QVariant::fromValue<Imap::Message::Envelope>(Imap::Message::Envelope(date, subject, from, sender, replyTo, to, cc, bcc, inReplyTo, messageId));
        }
    }*/

    switch (role) {
    case Imap::Mailbox::RoleIsUnavailable:
        return m_state == UNAVAILABLE;
    case Imap::Mailbox::RolePartData:
        return m_data;
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
        qDebug() << "Unknown Role requested:" << role << "(base:" << Imap::Mailbox::RoleBase << ")";
        //Q_ASSERT(0); // unknown role
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

    //TODO: check whether out message is affected or not
    qDebug() << "mapDataChanged" << topLeft.model() << "->" << Imap::Network::MsgPartNetAccessManager::pathToPart(root, topLeft.data(Imap::Mailbox::RolePartPathToPart).toString()).model();
    QString tlPath = topLeft.data(Imap::Mailbox::RolePartPathToPart).toString();
    QString brPath = bottomRight.data(Imap::Mailbox::RolePartPathToPart).toString();
    emit dataChanged(
                Imap::Network::MsgPartNetAccessManager::pathToPart(root, tlPath),
                Imap::Network::MsgPartNetAccessManager::pathToPart(root, brPath)
                );
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

QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(index.isValid());

    MessagePart* part = static_cast<MessagePart*>(index.internalPointer());
    Q_ASSERT(part);

    return part->data(role);
}

void MessageModel::insertSubtree(const QModelIndex& parent, const QVector<Common::MessagePart *> &children)
{
    //Q_ASSERT(rowCount(parent) == 0);
    beginInsertRows(parent, 0, children.size());
    if (parent.isValid()) {
        MessagePart* part = static_cast<MessagePart*>(parent.internalPointer());
        Q_ASSERT(part);
        for (int i = 0; i < children.size(); ++i) {
            part->setChild(i, 0, children[i]);
        }
        LocalMessagePart *localPart = dynamic_cast<LocalMessagePart*>(part);
        if (localPart)
            localPart->setFetchingState(LocalMessagePart::DONE);
    } else {
        m_rootPart = children.first();
    }
    endInsertRows();
}
}
