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

Common::LocalMessagePart* MimeEntityToPart(const mimetic::MimeEntity& me)
{
    QString type = QString(me.header().contentType().type().c_str());
    QString subtype = QString(me.header().contentType().subtype().c_str());
    Common::LocalMessagePart *part = new Common::LocalMessagePart(nullptr, QString("%1/%2").arg(type, subtype).toUtf8());
    if (me.body().parts().size() > 0)
    {
        int i = 0;
        Q_FOREACH(mimetic::MimeEntity* child, me.body().parts()) {
            Common::LocalMessagePart *childPart = MimeEntityToPart(*child);
            part->setChild(i++,0, childPart);
        }
    } else {
        QByteArray data(me.body().data(), me.body().size());
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
        Common::LocalMessagePart *part = MimeEntityToPart(me);
        QVector<Common::MessagePart*> children;
        for ( int i = 0; i < part->rowCount(); ++i ) {
            children.append(part->child(i, 0));
        }
        emit dataDecrypted(m_partIndex, children);
    }
#endif /* TROJITA_HAVE_QCA */
}

}
