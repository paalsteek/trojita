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

MessagePart::MessagePart(MessagePart *parent, int row)
    : m_parent(parent)
    , m_children()
    , m_row(row)
{
}

MessagePart::~MessagePart()
{
}

MessagePart* MessagePart::child(int row, int column) const
{
    //TODO: handle column
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

ProxyMessagePart::ProxyMessagePart(MessagePart *parent, int row, const QModelIndex& sourceIndex)
    : MessagePart(parent, row)
    , m_sourceIndex(sourceIndex)
{
}

ProxyMessagePart::~ProxyMessagePart()
{
}

LocalMessagePart::LocalMessagePart(MessagePart *parent, int row)
    : MessagePart(parent, row)
{
}

LocalMessagePart::~LocalMessagePart()
{
}

QVariant LocalMessagePart::data(int role) const
{
    if (role == Imap::Mailbox::RoleIsFetched) {
        return true;
    }

    /*switch (role) {
    case Qt::DisplayRole:
        if (isTopLevelMultipart())
            return QString("%1").arg(mimetype());
        else
            return QString("%1: %2").arg(partId(), mimetype());
    case Qt::ToolTipRole:
    {
        mimetic::Body b = m_me->body();
        return b.size() > 10000 ? QString("%1 bytes of data").arg(b.size()) : b.data();
    }
    }*/

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
        return false;
    case Imap::Mailbox::RolePartData:
        return m_data;
    /*case Imap::Mailbox::RolePartMimeType:
        return mimetype();
    case Imap::Mailbox::RolePartCharset:
        return charset();
    case Imap::Mailbox::RolePartContentFormat:
        return format();
    case Imap::Mailbox::RolePartContentDelSp:
        return delsp();
    case Imap::Mailbox::RolePartEncoding:
        return transferEncoding();
    case Imap::Mailbox::RolePartBodyFldId:
        return bodyFldId();
    case Imap::Mailbox::RolePartBodyDisposition:
        return bodyDisposition();
    case Imap::Mailbox::RolePartFileName:
        return filename();
    case Imap::Mailbox::RolePartOctets:
        return octets();
    case Imap::Mailbox::RolePartId:
        return partId();
    case Imap::Mailbox::RolePartPathToPart:
        return pathToPart();
    case Imap::Mailbox::RolePartMultipartRelatedMainCid:
        if (relatedMainCid().isEmpty()) {
            return relatedMainCid();
        } else {
            return QVariant();
        }
    case Imap::Mailbox::RolePartIsTopLevelMultipart:
        return isTopLevelMultipart();*/
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
    , m_factory(new MessagePartFactory())
{
    connect(message.model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SIGNAL(dataChanged(QModelIndex,QModelIndex)));
    m_factory->buildSubtree(message, this);
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
        child = part->child(row);
    }

    if (!child)
        return QModelIndex();

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
    return part->rowCount();
}

QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    Q_ASSERT(index.isValid());

    MessagePart* part = static_cast<MessagePart*>(index.internalPointer());
    Q_ASSERT(part);

    return part->data(role);
}

void MessageModel::insertSubtree(const QModelIndex& parent, int row, int column, const QVector<MessagePart *> &children)
{
    beginInsertRows(parent, row, row + children.size());
    if (parent.isValid()) {
        MessagePart* part = static_cast<MessagePart*>(parent.internalPointer());
        Q_ASSERT(part);
        for (int i = 0; i < children.size(); ++i) {
            part->setChild(row + i, column, children[i]);
        }
    } else {
        m_rootPart = children.first();
    }
    endInsertRows();
}
}
