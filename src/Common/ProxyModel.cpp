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

#include "ProxyModel.h"
#include "Imap/Encoders.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include <QDebug>

#include "Cryptography/OpenPGPHelper.h"
#include "Cryptography/SMIMEHelper.h"

namespace Common {

//#ifdef TROJITA_HAVE_GNUPG
QCA::Initializer init;
//#endif // TROJITA_HAVE_GNUPG

MessagePart::MessagePart(MessagePart *parent, int row)
    : QObject()
    , m_parent(parent)
    , m_row(row)
{
}

MessagePart::~MessagePart()
{
}

MessagePart* MessagePart::child(int row)
{
    if ( !children.contains(row) ) {
        children.insert(row, newChild(row));
    }
    return children[row];
}

void MessagePart::replaceChild(int row, MessagePart *part)
{
    Q_ASSERT(children.contains(row));

    children.insert(row, part);
}

ProxyMessagePart::ProxyMessagePart(MessagePart *parent, int row, const QModelIndex& sourceIndex)
    : MessagePart(parent, row)
    , m_sourceIndex(sourceIndex)
{
    connect(sourceIndex.model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleSourceDataChanged(QModelIndex,QModelIndex)));
}

ProxyMessagePart::~ProxyMessagePart()
{
}

MessagePart* ProxyMessagePart::newChild(int row)
{
    if (row >= rowCount())
        return nullptr;

    ProxyMessagePart* child = new ProxyMessagePart(this, row, m_sourceIndex.child(row, 0));
    return child;
}

int ProxyMessagePart::rowCount() const
{
    return m_sourceIndex.model()->rowCount(m_sourceIndex);
}

void ProxyMessagePart::handleSourceDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight)
{
    Q_ASSERT(topLeft.parent() == bottomRight.parent());
    if (topLeft.parent() == m_sourceIndex.parent()
            && topLeft.row() <= m_sourceIndex.row()
            && m_sourceIndex.row() <= bottomRight.row()) {
        emit partChanged();
    }
}

LocalMessagePart::LocalMessagePart(MessagePart *parent, int row, mimetic::MimeEntity *pMe)
    : MessagePart(parent, row)
    , m_me(pMe)
{
}

LocalMessagePart::~LocalMessagePart()
{
}

MessagePart* LocalMessagePart::newChild(int row)
{
    Q_ASSERT(m_me);
    if (row >= rowCount())
        return nullptr;

    mimetic::MimeEntityList::iterator it = m_me->body().parts().begin();
    for (int i = 0; i < row; i++) { it++; }
    MessagePart* child = new LocalMessagePart(this, row, *it);
    return child;
}

int LocalMessagePart::rowCount() const
{
    if (!m_me)
        return 0;

    return m_me->body().parts().size();
}

QString LocalMessagePart::mimetype() const
{
    Q_ASSERT(m_me);

    QString type = QString::fromStdString(m_me->header().contentType().type());
    QString subtype = QString::fromStdString(m_me->header().contentType().subtype());

    return QString("%1/%2").arg(type, subtype);
}

QByteArray LocalMessagePart::data() const
{
    Q_ASSERT(m_me);

    QByteArray rawData = QByteArray::fromRawData(m_me->body().data(), m_me->body().size());
    qDebug() << "raw partData:" << rawData;
    QByteArray data;
    Imap::decodeContentTransferEncoding(rawData, transferEncoding(), &data);
    return data;
}

int LocalMessagePart::octets() const
{
    Q_ASSERT(m_me);

    return m_me->size();
}

QString LocalMessagePart::charset() const
{
    Q_ASSERT(m_me);

    return QString::fromStdString(m_me->header().contentType().param("charset"));
}

QString LocalMessagePart::protocol() const
{
    Q_ASSERT(m_me);

    return QString::fromStdString(m_me->header().contentType().param("protocol"));
}

QString LocalMessagePart::filename() const
{
    Q_ASSERT(m_me);

    return QString::fromStdString(m_me->header().contentType().param("name"));
}

QString LocalMessagePart::format() const
{
    Q_ASSERT(m_me);

    return QString::fromStdString(m_me->header().contentType().param("format"));
}

QString LocalMessagePart::delsp() const
{
    Q_ASSERT(m_me);

    return QString::fromStdString(m_me->header().contentType().param("delsp"));
}

QByteArray LocalMessagePart::transferEncoding() const
{
    Q_ASSERT(m_me);

    std::string rawEncoding = m_me->header().contentTransferEncoding().str();
    return QByteArray::fromRawData(rawEncoding.c_str(), rawEncoding.size());
}

QByteArray LocalMessagePart::bodyDisposition() const
{
    Q_ASSERT(m_me);

    std::string rawBodyDisposition = m_me->header().contentDisposition().type();
    return QByteArray::fromRawData(rawBodyDisposition.c_str(), rawBodyDisposition.size());
}

QVariant LocalMessagePart::data(int role)
{
    if (role == Imap::Mailbox::RoleIsFetched) {
        return !!m_me;
    }

    if (!m_me)
        return QVariant();

    switch (role) {
    case Imap::Mailbox::RolePartData:
        return data();
    case Imap::Mailbox::RolePartMimeType:
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
        Q_ASSERT(0); // TODO: implement this
        break;
    case Imap::Mailbox::RolePartBodyDisposition:
        return bodyDisposition();
    case Imap::Mailbox::RolePartFileName:
        return filename();
    case Imap::Mailbox::RolePartOctets:
        return octets();
    case Imap::Mailbox::RolePartId:
        // TODO: handling of TopLevelMultiparts (see TreeItemPart::partId())
        return QString::number(row() + 1);
    case Imap::Mailbox::RolePartPathToPart:
    {
        // This item is not directly fetcheable, so it does *not* make sense to ask for it.
        // We cannot really assert at this point, though, because this function is published via the MVC interface.
        //return QLatin1String("application-bug-dont-fetch-this");
        // TODO: do we need some way to prevent fetching of local parts?
        QString parentPath = QLatin1String("");
        if (parent()) {
            parentPath = parent()->data(Imap::Mailbox::RolePartPathToPart).toString();
        }
        qDebug() << "PathToPart:" << parentPath + QChar('/') + QString::number(row() + 1);
        return parentPath + QChar('/') + QString::number(row() + 1);
    }
    case Imap::Mailbox::RolePartMultipartRelatedMainCid:
        Q_ASSERT(0); // TODO: implement this
        break;
    case Imap::Mailbox::RolePartIsTopLevelMultipart:
        return false; // TODO: check meaning of TopLevelMultipart
    case Imap::Mailbox::RolePartForceFetchFromCache:
        return QVariant(); // Nothing to do here
    default:
        Q_ASSERT(0); // unknown role
        break;
    }

    return QVariant();
}

EncryptedMessagePart::EncryptedMessagePart(MessagePart *parent, int row, MessagePart *raw)
    : LocalMessagePart(parent, row, NULL)
    , m_raw(raw)
{
}

void EncryptedMessagePart::handleDataDecrypted(mimetic::MimeEntity *pMe)
{
    disconnect(sender(), SIGNAL(dataDecrypted(mimetic::MimeEntity*)), this, SLOT(handleDataDecrypted(mimetic::MimeEntity*)));
    m_me = pMe;
    emit partDecrypted();
    emit partChanged();
}

MessageModel::MessageModel(QObject *parent, const QModelIndex &message)
    : QAbstractItemModel(parent)
    , m_message(message)
    , m_rootPart(0)
    , m_pgpHelper(new Cryptography::OpenPGPHelper(this))
    , m_smimeHelper(new Cryptography::SMIMEHelper(this))
{
}

MessageModel::~MessageModel()
{
}

QModelIndex MessageModel::index(int row, int column, const QModelIndex &parent) const
{
    MessagePart *part, *child;
    if (!parent.isValid()) {
        part = 0;
        if (!m_rootPart) {
            m_rootPart = new ProxyMessagePart(part, 0, m_message.child(0,0));
            connect(m_rootPart, SIGNAL(partChanged()), this, SLOT(handlePartChanged()));
        }
        child = m_rootPart;
    } else {
        part = static_cast<MessagePart*>(parent.internalPointer());
        Q_ASSERT(part);
        if (column != 0) {
            EncryptedMessagePart* encPart = qobject_cast<EncryptedMessagePart*>(part);
            Q_ASSERT(encPart);
            return createIndex(row, column, encPart->rawPart());
        }
        if (row >= rowCount(parent)) {
            return QModelIndex();
        }
        child = part->child(row);
    }

    if (!child)
        return QModelIndex();

    if (child->data(Imap::Mailbox::RolePartMimeType).toString().toLower() == QLatin1String("multipart/encrypted")) {
        EncryptedMessagePart* encPart = new EncryptedMessagePart(part, row, child);
        if (child == m_rootPart) {
            m_rootPart = encPart;
        } else if (part) {
            part->replaceChild(row, encPart);
        }
        connect(m_pgpHelper, SIGNAL(dataDecrypted(mimetic::MimeEntity*)), encPart, SLOT(handleDataDecrypted(mimetic::MimeEntity*)));
        connect(encPart, SIGNAL(partChanged()), this, SLOT(handlePartChanged()));
        connect(encPart, SIGNAL(partDecrypted()), this, SLOT(handlePartDecrypted()));
        QModelIndex index = createIndex(row, column, encPart);
        m_pgpHelper->decrypt(index);
        return index;
    }

    connect(child, SIGNAL(partChanged()), this, SLOT(handlePartChanged()));

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

void MessageModel::handlePartChanged()
{
    MessagePart* part = qobject_cast<MessagePart*>(sender());
    QModelIndex index = createIndex(part->row(), 0, part);
    emit dataChanged(index, index);
}

void MessageModel::handlePartDecrypted()
{
    MessagePart* part = qobject_cast<MessagePart*>(sender());
    QModelIndex index = createIndex(part->row(), 0, part);
    beginInsertRows(index, 0, 1); // have to call this after the modification as otherwise rowCount is called before we updated m_parts
    endInsertRows();
}
}
