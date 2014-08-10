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

#include <mimetic/mimetic.h>

#include <sstream>
#include "QCAHelper.h"
#include "MessagePart.h"
#include "Imap/Encoders.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Parser/Message.h"
#include "Imap/Parser/Rfc5322HeaderParser.h"

namespace Cryptography {
QCAHelper::QCAHelper(QObject *parent)
    : QObject(parent)
    , m_handler(this)
{
    connect(&m_handler, SIGNAL(eventReady(int,QCA::Event)), this, SLOT(handleEventReady(int,QCA::Event)));
    m_handler.start();
}

QString QCAHelper::qcaErrorStrings(int e)
{
    switch (e) {
    case QCA::SecureMessage::ErrorPassphrase:
        return tr("passphrase was either wrong or not provided");
    case QCA::SecureMessage::ErrorFormat:
        return tr("input format was bad");
    case QCA::SecureMessage::ErrorSignerExpired:
        return tr("signing key is expired");
    case QCA::SecureMessage::ErrorSignerInvalid:
        return tr("signing key is invalid in some way");
    case QCA::SecureMessage::ErrorEncryptExpired:
        return tr("encrypting key is expired");
    case QCA::SecureMessage::ErrorEncryptUntrusted:
        return tr("encrypting key is untrusted");
    case QCA::SecureMessage::ErrorEncryptInvalid:
        return tr("encrypting key is invalid in some way");
    case QCA::SecureMessage::ErrorNeedCard:
        return tr("pgp card is missing");
    case QCA::SecureMessage::ErrorCertKeyMismatch:
        return tr("certificate and private key don't match");
    case QCA::SecureMessage::ErrorUnknown:
    default:
        return tr("other error (%1)").arg(e);
    }
}

void QCAHelper::handleEventReady(int id, const QCA::Event &e)
{
    if ( e.isNull() ) {
        return;
    }

    if ( e.type() == QCA::Event::Password ) {
        emit passwordRequired(id, e.fileName());
    } else {
        m_handler.reject(id);
    }
}

void QCAHelper::handlePassword(int id, const QString &password)
{
    m_handler.submitPassword(id, QCA::SecureArray(password.toLocal8Bit()));
}

void QCAHelper::handlePasswordError(int id)
{
    m_handler.reject(id);
}

void QCAHelper::storeInterestingFields(const mimetic::MimeEntity& me, LocalMessagePart* part)
{
    part->setCharset(QString::fromStdString(me.header().contentType().param("charset")).toLocal8Bit());
    QString format = QString::fromStdString(me.header().contentType().param("format"));
    if (!format.isEmpty()) {
        part->setContentFormat(format.toLocal8Bit());
        part->setDelSp(QString::fromStdString(me.header().contentType().param("delsp")).toLocal8Bit());
    }
    const mimetic::ContentDisposition& cd = me.header().contentDisposition();
    QByteArray bodyDisposition = QString::fromStdString(cd.type()).toLocal8Bit();
    QString filename;
    if (!bodyDisposition.isEmpty()) {
        part->setBodyDisposition(bodyDisposition);
        std::list<mimetic::ContentDisposition::Param> l = cd.paramList();
        QMap<QByteArray, QByteArray> paramMap;
        for (auto it = l.begin(); it != l.end(); ++it) {
            const mimetic::ContentDisposition::Param& p = *it;
            paramMap.insert(QString::fromStdString(p.name()).toLocal8Bit(), QString::fromStdString(p.value()).toLocal8Bit());
        }
        filename = Imap::extractRfc2231Param(paramMap, "filename");
    }
    if (filename.isEmpty()) {
        std::list<mimetic::ContentType::Param> l = me.header().contentType().paramList();
        QMap<QByteArray, QByteArray> paramMap;
        for (auto it = l.begin(); it != l.end(); ++it) {
            const mimetic::ContentType::Param& p = *it;
            paramMap.insert(QString::fromStdString(p.name()).toLocal8Bit(), QString::fromStdString(p.value()).toLocal8Bit());
        }
        filename = Imap::extractRfc2231Param(paramMap, "name");
    }

    if (!filename.isEmpty()) {
        part->setFilename(filename);
    }

    part->setEncoding(QString::fromStdString(me.header().contentTransferEncoding().str()).toLocal8Bit());
    part->setBodyFldId(QString::fromStdString(me.header().contentId().str()).toLocal8Bit());
}

QList<Imap::Message::MailAddress> QCAHelper::mailboxListToQList(const mimetic::MailboxList& list) {
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

QList<Imap::Message::MailAddress> QCAHelper::addressListToQList(const mimetic::AddressList& list) {
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

QByteArray dumpMimeHeader(const mimetic::Header& h)
{
    std::stringstream ss;
    for (auto it = h.begin(), end = h.end(); it != end; ++it) {
        it->write(ss) << "\r\n";
    }
    return QString::fromStdString(ss.str()).toLocal8Bit();
}

LocalMessagePart* QCAHelper::mimeEntityToPart(const mimetic::MimeEntity& me)
{
    QString type = QLatin1String(me.header().contentType().type().c_str());
    QString subtype = QLatin1String(me.header().contentType().subtype().c_str());
    LocalMessagePart *part = new LocalMessagePart(nullptr, QString::fromUtf8("%1/%2").arg(type, subtype).toUtf8());
    if (me.body().parts().size() > 0)
    {
        if (type == QLatin1String("message") && subtype == QLatin1String("rfc822")) {
            mimetic::Header h = (*(me.body().parts().begin()))->header();
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
            Imap::LowLevelParser::Rfc5322HeaderParser headerParser;
            QByteArray rawHeader = dumpMimeHeader(h);
            bool ok = headerParser.parse(rawHeader);
            if (!ok) {
                qDebug() << QLatin1String("Unspecified error during RFC5322 header parsing");
            } else {
                part->setHdrReferences(headerParser.references);
                if (!headerParser.listPost.isEmpty()) {
                    QList<QUrl> listPost;
                    Q_FOREACH(QByteArray item, headerParser.listPost)
                        listPost << QUrl(QString::fromUtf8(item));
                    part->setHdrListPost(listPost);
                }
                if (headerParser.listPostNo)
                    part->setHdrListPostNo(true);
            }
            QByteArray messageId = headerParser.messageId.size() == 1 ? headerParser.messageId.front() : QByteArray();
            part->setEnvelope(new Imap::Message::Envelope(date, subject, from, sender, replyTo, to, cc, bcc, headerParser.inReplyTo, messageId));
        }
        int i = 0;
        Q_FOREACH(mimetic::MimeEntity* child, me.body().parts()) {
            LocalMessagePart *childPart = mimeEntityToPart(*child);
            part->setChild(i++,0, childPart);
        }
        // Remember full part data for part download
        std::stringstream ss;
        ss << me;
        part->setData(QString::fromStdString(ss.str()).toLocal8Bit());
    } else {
        storeInterestingFields(me, part);
        QByteArray data;
        Imap::decodeContentTransferEncoding(QByteArray(me.body().data(), me.body().size()), part->data(Imap::Mailbox::RolePartEncoding).toByteArray(), &data);
        part->setData(data);
    }
    part->setOctets(me.size());
    part->setFetchingState(LocalMessagePart::DONE);
    return part;
}

}
