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

#include "configure.cmake.h"
#ifdef TROJITA_HAVE_MIMETIC
#include <mimetic/mimetic.h>
#endif /* TROJITA_HAVE_MIMETIC */

#include "QCAHelper.h"
#include "MessageModel.h"
#include "Imap/Encoders.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Parser/Message.h"

namespace Cryptography {
QCAHelper::QCAHelper(QObject *parent)
    : QObject(parent)
    , _handler(this)
{
    connect(&_handler, SIGNAL(eventReady(int,QCA::Event)), this, SLOT(handleEventReady(int,QCA::Event)));
    _handler.start();
}

QString QCAHelper::qcaErrorStrings(int e)
{
#ifdef TROJITA_HAVE_QCA
    QMap<int, QString> map;
    map[QCA::SecureMessage::ErrorPassphrase] = tr("passphrase was either wrong or not provided");
    map[QCA::SecureMessage::ErrorFormat] = tr("input format was bad");
    map[QCA::SecureMessage::ErrorSignerExpired] = tr("signing key is expired");
    map[QCA::SecureMessage::ErrorSignerInvalid] = tr("signing key is invalid in some way");
    map[QCA::SecureMessage::ErrorEncryptExpired] = tr("encrypting key is expired");
    map[QCA::SecureMessage::ErrorEncryptUntrusted] = tr("encrypting key is untrusted");
    map[QCA::SecureMessage::ErrorEncryptInvalid] = tr("encrypting key is invalid in some way");
    map[QCA::SecureMessage::ErrorNeedCard] = tr("pgp card is missing");
    map[QCA::SecureMessage::ErrorCertKeyMismatch] = tr("certificate and private key don't match");
    map[QCA::SecureMessage::ErrorUnknown] = tr("other error");
    return map[e];
#else
    Q_UNUSED(e);
    return tr("Trojitá is missing support for OpenPGP.");
#endif /* TROJITA_HAVE_QCA */
}

void QCAHelper::handleEventReady(int id, const QCA::Event &e)
{
    if ( e.isNull() ) {
        return;
    }

    if ( e.type() == QCA::Event::Password ) {
        // TODO: hard coded password? really?
        _handler.submitPassword(id, QCA::SecureArray(""));
    } else {
        _handler.reject(id);
    }
}

#ifdef TROJITA_HAVE_MIMETIC
void QCAHelper::storeInterestingFields(const mimetic::MimeEntity& me, LocalMessagePart* part)
{
    part->setCharset(QString::fromStdString(me.header().contentType().param("charset")));
    QString format = QString::fromStdString(me.header().contentType().param("format"));
    if (!format.isEmpty()) {
        part->setContentFormat(format);
        part->setDelSp(QString::fromStdString(me.header().contentType().param("delsp")));
    }
    const mimetic::ContentDisposition& cd = me.header().contentDisposition();
    QByteArray bodyDisposition(cd.type().c_str(), cd.type().size());
    QString filename;
    if (!bodyDisposition.isEmpty()) {
        part->setBodyDisposition(bodyDisposition);
        std::list<mimetic::ContentDisposition::Param> l = cd.paramList();
        QMap<QByteArray, QByteArray> paramMap;
        for (auto it = l.begin(); it != l.end(); ++it) {
            const mimetic::ContentDisposition::Param& p = *it;
            paramMap.insert(QByteArray(p.name().c_str(), p.name().size()), QByteArray(p.value().c_str(), p.value().size()));
        }
        filename = Imap::extractRfc2231Param(paramMap, "filename");
    }
    if (filename.isEmpty()) {
        std::list<mimetic::ContentType::Param> l = me.header().contentType().paramList();
        QMap<QByteArray, QByteArray> paramMap;
        for (auto it = l.begin(); it != l.end(); ++it) {
            const mimetic::ContentType::Param& p = *it;
            paramMap.insert(QByteArray(p.name().c_str(), p.name().size()), QByteArray(p.value().c_str(), p.value().size()));
        }
        filename = Imap::extractRfc2231Param(paramMap, "name");
    }

    if (!filename.isEmpty()) {
        part->setFilename(filename);
    }

    part->setEncoding(QByteArray(me.header().contentTransferEncoding().str().c_str(), me.header().contentTransferEncoding().str().size()));
    part->setBodyFldId(QByteArray(me.header().contentId().str().c_str(), me.header().contentId().str().size()));
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

LocalMessagePart* QCAHelper::mimeEntityToPart(const mimetic::MimeEntity& me)
{
    QString type = QString(me.header().contentType().type().c_str());
    QString subtype = QString(me.header().contentType().subtype().c_str());
    LocalMessagePart *part = new LocalMessagePart(nullptr, QString("%1/%2").arg(type, subtype).toUtf8());
    if (me.body().parts().size() > 0)
    {
        if (!type.compare("message") && !subtype.compare("rfc822")) {
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
            QList<QByteArray> inReplyTo;
            if (h.hasField("In-Reply-To")) {
                inReplyTo.append(QByteArray(h.field("In-Reply-To").value().c_str(), h.field("In-Reply-To").value().length()));
            }
            QByteArray messageId = QByteArray::fromRawData(h.messageid().str().c_str(), h.messageid().str().length());
            part->setEnvelope(new Imap::Message::Envelope(date, subject, from, sender, replyTo, to, cc, bcc, inReplyTo, messageId));
        }
        int i = 0;
        Q_FOREACH(mimetic::MimeEntity* child, me.body().parts()) {
            LocalMessagePart *childPart = mimeEntityToPart(*child);
            part->setChild(i++,0, childPart);
        }
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
#endif /* TROJITA_HAVE_MIMETIC */

}
