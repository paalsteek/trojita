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
    : QObject()
    , m_parent(parent)
    , m_factory(new MessagePartFactory())
    , m_row(row)
{
    //TODO: singleton for messagepartfactory?
    connect(this, SIGNAL(needChild(int,int)), m_factory, SLOT(createPart(int,int)));
    connect(m_factory, SIGNAL(newPart(int,int,MessagePart*)), this, SLOT(addChild(int,int,MessagePart*)));
}

MessagePart::~MessagePart()
{
}

MessagePart* MessagePart::child(int row)
{
    if ( !m_children.contains(row) ) {
        emit needChild(row, 0);
        return nullptr;
    }
    return m_children[row];
}

void MessagePart::replaceChild(int row, MessagePart *part)
{
    Q_ASSERT(m_children.contains(row));

    m_children.insert(row, part);
}

void MessagePart::addChild(int row, int column, MessagePart *part)
{
    Q_UNUSED(column);
    if (m_children.contains(row))
        return;
    emit aboutToBeInserted(row, 1);
    m_children.insert(row, part);
    emit endInsert();
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

#ifdef TROJITA_HAVE_MIMETIC
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

    QString type = QString(m_me->header().contentType().type().c_str());
    QString subtype = QString(m_me->header().contentType().subtype().c_str());

    return QString("%1/%2").arg(type, subtype);
}

QByteArray LocalMessagePart::data() const
{
    Q_ASSERT(m_me);

    QByteArray rawData = QByteArray::fromRawData(m_me->body().data(), m_me->body().size());
    if (!m_me->body().parts().empty()) {
        std::stringstream ss;
        for(mimetic::MimeEntityList::iterator it = m_me->body().parts().begin(), end = m_me->body().parts().end(); it != end; it++)
        {
            ss << **it;
        }
        rawData = QByteArray::fromRawData(ss.str().c_str(), ss.str().length());
    }
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

    return QString(m_me->header().contentType().param("protocol").c_str());
}

QString LocalMessagePart::filename() const
{
    Q_ASSERT(m_me);

    QString filename = QString(m_me->header().contentDisposition().param("filename").c_str());
    if (filename.isEmpty()) {
        std::string rawstr = m_me->header().contentDisposition().param("filename*");
        QByteArray raw = QByteArray::fromRawData(rawstr.c_str(), rawstr.length());

        filename = Imap::decodeRFC2231String(raw);
    }
    if (filename.isEmpty())
        filename = QString(m_me->header().contentType().param("name").c_str());
    return filename;
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

QByteArray LocalMessagePart::bodyFldId() const
{
    Q_ASSERT(m_me);

    std::string rawBodyFldId = m_me->header().contentId().str();
    return QByteArray::fromRawData(rawBodyFldId.c_str(), rawBodyFldId.size());
}

QByteArray LocalMessagePart::relatedMainCid() const
{
    Q_ASSERT(m_me);

    std::string rawMainCid = m_me->header().contentType().param("start");
    return QByteArray::fromRawData(rawMainCid.c_str(), rawMainCid.size());
}

QString LocalMessagePart::partId() const {
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
    return parentPath + QChar('/') + QString::number(row());
}

bool LocalMessagePart::isTopLevelMultipart() const {
    return mimetype().startsWith("multipart/") && (!parent()->parent() || parent()->data(Imap::Mailbox::RolePartMimeType).toString().startsWith("message/"));
}

QList<Imap::Message::MailAddress> mailboxListToQList(const mimetic::MailboxList& list) {
    QList<Imap::Message::MailAddress> result;
    Q_FOREACH(mimetic::Mailbox addr, list)
    {
        result.append(Imap::Message::MailAddress(
                        QString::fromStdString(addr.label()),
                        QString::fromStdString(addr.sourceroute()),
                        QString::fromStdString(addr.mailbox()),
                        QString::fromStdString(addr.domain())));
    }
    return result;
}

QList<Imap::Message::MailAddress> addressListToQList(const mimetic::AddressList& list) {
    QList<Imap::Message::MailAddress> result;
    Q_FOREACH(mimetic::Address addr, list)
    {
        if (addr.isGroup()) {
            mimetic::Group group = addr.group();
            for (mimetic::Group::iterator it = group.begin(), end = group.end(); it != end; it++) {
                mimetic::Mailbox mb = *it;
                result.append(Imap::Message::MailAddress(
                                  QString::fromStdString(mb.label()),
                                  QString::fromStdString(mb.sourceroute()),
                                  QString::fromStdString(mb.mailbox()),
                                  QString::fromStdString(mb.domain())));
            }
        } else {
            mimetic::Mailbox mb = addr.mailbox();
            result.append(Imap::Message::MailAddress(
                              QString::fromStdString(mb.label()),
                              QString::fromStdString(mb.sourceroute()),
                              QString::fromStdString(mb.mailbox()),
                              QString::fromStdString(mb.domain())));
        }
    }
    return result;
}

QVariant LocalMessagePart::data(int role)
{
    if (role == Imap::Mailbox::RoleIsFetched) {
        return !!m_me;
    }

    if (!m_me)
        return QVariant();

    switch (role) {
    case Qt::DisplayRole:
        if (isTopLevelMultipart())
            return QString("%1").arg(mimetype());
        else
            return QString("%1: %2").arg(partId(), mimetype());
    case Qt::ToolTipRole:
    {
        mimetic::Body b = m_me->body();
        return b.size() > 10000 ? tr("%1 bytes of data").arg(b.size()) : b.data();
    }
    }

    if (mimetype().toLower() == QLatin1String("message/rfc822")) {
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
    }

    switch (role) {
    case Imap::Mailbox::RoleIsUnavailable:
        return false;
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

EncryptedMessagePart::EncryptedMessagePart(MessagePart *parent, int row, MessagePart *raw)
    : LocalMessagePart(parent, row, nullptr)
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
#endif /* TROJITA_HAVE_MIMETIC */

MessageModel::MessageModel(QObject *parent, const QModelIndex &message)
    : QAbstractItemModel(parent)
    , m_message(message)
    , m_rootPart(nullptr)
    , m_pgpHelper(new Cryptography::OpenPGPHelper(this))
    , m_smimeHelper(new Cryptography::SMIMEHelper(this))
{
    connect(m_pgpHelper, SIGNAL(decryptionFailed(QString)), this, SIGNAL(decryptionFailed(QString)));
}

MessageModel::~MessageModel()
{
}

QModelIndex MessageModel::index(int row, int column, const QModelIndex &parent) const
{
    MessagePart* child;
    if (!parent.isValid()) {
        if (!m_rootPart) {
            m_rootPart = new ProxyMessagePart(nullptr, 0, m_message);
        }
        child = m_rootPart;
    } else {
        MessagePart* part = static_cast<MessagePart*>(parent.internalPointer());
        Q_ASSERT(part);
        child = part->child(row);
    }

    if (!child)
        return QModelIndex();

    connect(child, SIGNAL(partChanged()), this, SLOT(handlePartChanged()), Qt::UniqueConnection);
    connect(child, SIGNAL(aboutToBeInserted(int,int)), this, SLOT(handleAboutToBeInserted(int,int)), Qt::UniqueConnection);
    connect(child, SIGNAL(endInsert()), this, SLOT(handleEndInsert()), Qt::UniqueConnection);

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

void MessageModel::handleAboutToBeInserted(int row, int count)
{
    MessagePart* part = qobject_cast<MessagePart*>(sender());
    QModelIndex index = createIndex(part->row(), 0, part);
    beginInsertRows(index, row, count);
}

void MessageModel::handleEndInsert()
{
    qDebug() << "MessageModel::handleEndInsert()";
    endInsertRows();
}

void MessageModel::handlePartDecrypted()
{
    MessagePart* part = qobject_cast<MessagePart*>(sender());
    QModelIndex index = createIndex(part->row(), 0, part);
    beginInsertRows(index, 0, 1); // have to call this after the modification as otherwise rowCount is called before we updated m_parts
    endInsertRows();
}
}
