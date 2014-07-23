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

#include "OpenPGPHelper.h"

#include <QDebug>

#include "configure.cmake.h"
#include "Common/ProxyModel.h"
#include "Imap/Encoders.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"

#ifdef TROJITA_HAVE_MIMETIC
#include <mimetic/mimetic.h>
#endif /* TROJITA_HAVE_MIMETIC */

#ifdef TROJITA_HAVE_QCA
#include <QtCrypto/QtCrypto>
#endif /* TROJITA_HAVE_QCA */

namespace Cryptography {
QString OpenPGPHelper::qcaErrorStrings(int e)
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

OpenPGPHelper::OpenPGPHelper(QObject *parent)
    : QObject(parent)
    , m_partIndex()
#ifdef TROJITA_HAVE_QCA
    , m_pgp(new QCA::OpenPGP(this))
#endif /* TROJITA_HAVE_GNUPG */
{
}

OpenPGPHelper::~OpenPGPHelper()
{
}

void OpenPGPHelper::decrypt(const QModelIndex &parent)
{
#ifdef TROJITA_HAVE_QCA
    if (QCA::isSupported("openpgp")) {
        m_partIndex = parent;
        QModelIndex sourceIndex = m_partIndex.child(0, Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS);
        Q_ASSERT(sourceIndex.isValid());
        Q_ASSERT(sourceIndex.model()->rowCount(sourceIndex) == 2);
        connect(sourceIndex.model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChanged(QModelIndex,QModelIndex)));
        //Trigger lazy loading of the required message parts
        sourceIndex.child(0,0).data(Imap::Mailbox::RolePartData);
        sourceIndex.child(1,0).data(Imap::Mailbox::RolePartData);
        //call handleDataChanged at least once in case all parts are already available
        handleDataChanged(QModelIndex(),QModelIndex());
    } else
#endif /* TROJITA_HAVE_QCA */
    emit decryptionFailed(qcaErrorStrings(0));
}

void OpenPGPHelper::handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
#ifdef TROJITA_HAVE_QCA
    Q_UNUSED(topLeft)
    Q_UNUSED(bottomRight)
    QModelIndex sourceIndex = m_partIndex.child(0, Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS);
    if ( sourceIndex.child(0,0).data(Imap::Mailbox::RoleIsFetched).toBool() &&
         sourceIndex.child(1,0).data(Imap::Mailbox::RoleIsFetched).toBool() )
    {
        disconnect(sourceIndex.model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChanged(QModelIndex,QModelIndex)));

        QModelIndex versionIndex = sourceIndex.child(0,0), encIndex = sourceIndex.child(1,0);
        if ( !versionIndex.data(Imap::Mailbox::RolePartData).toString().contains(QLatin1String("Version: 1")) )
        {
            emit decryptionFailed(tr("Unable to decrypt message: Unsupported PGP/MIME version"));
            return;
        }
        QCA::SecureMessage* msg = new QCA::SecureMessage(m_pgp);
        connect(msg, SIGNAL(finished()), this, SLOT(decryptionFinished()));
        msg->setFormat(QCA::SecureMessage::Ascii);
        msg->startDecrypt();
        msg->update(encIndex.data(Imap::Mailbox::RolePartData).toByteArray().data());
        msg->end();
    }
#endif /* TROJITA_HAVE_QCA */
}

void OpenPGPHelper::storeInterestingFields(const mimetic::MimeEntity& me, Common::LocalMessagePart* part)
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

Common::LocalMessagePart* OpenPGPHelper::mimeEntityToPart(const mimetic::MimeEntity& me)
{
    QString type = QString(me.header().contentType().type().c_str());
    QString subtype = QString(me.header().contentType().subtype().c_str());
    Common::LocalMessagePart *part = new Common::LocalMessagePart(nullptr, QString("%1/%2").arg(type, subtype).toUtf8());
    if (me.body().parts().size() > 0)
    {
        int i = 0;
        Q_FOREACH(mimetic::MimeEntity* child, me.body().parts()) {
            Common::LocalMessagePart *childPart = mimeEntityToPart(*child);
            part->setChild(i++,0, childPart);
        }
    } else {
        storeInterestingFields(me, part);
        QByteArray data;
        Imap::decodeContentTransferEncoding(QByteArray(me.body().data(), me.body().size()), part->data(Imap::Mailbox::RolePartEncoding).toByteArray(), &data);
        part->setData(data);
    }
    part->setOctets(me.size());
    part->setFetchingState(Common::LocalMessagePart::DONE);
    return part;
}

void OpenPGPHelper::decryptionFinished()
{
#ifdef TROJITA_HAVE_QCA
    QCA::SecureMessage* msg = qobject_cast<QCA::SecureMessage*>(sender());
    Q_ASSERT(msg);

    if (!msg->success()) {
        emit decryptionFailed(qcaErrorStrings(msg->errorCode()));
        qDebug() << "Decryption Failed:" << qcaErrorStrings(msg->errorCode())
                    << msg->diagnosticText();
    } else {
        QByteArray message;
        while ( msg->bytesAvailable() )
        {
            message.append(msg->read());
        }
        qDebug() << message;
        mimetic::MimeEntity me(message.begin(), message.end());
        Common::LocalMessagePart *part = mimeEntityToPart(me);
        QVector<Common::MessagePart*> children;
        for ( int i = 0; i < part->rowCount(); ++i ) {
            children.append(part->child(i, 0));
        }
        emit dataDecrypted(m_partIndex, children);
    }
#endif /* TROJITA_HAVE_QCA */
}

}
