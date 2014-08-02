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

#include <QFile>
#ifdef TROJITA_HAVE_MIMETIC
#include <mimetic/mimetic.h>
#endif /* TROJITA_HAVE_MIMETIC */

#include "SMIMEHelper.h"
#include "MessageModel.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"


namespace Cryptography {
SMIMEHelper::SMIMEHelper(QObject *parent)
    : QCAHelper(parent)
    , m_cms(new QCA::CMS(this))
    , m_loader(new QCA::KeyLoader(this))
{
    if (QCA::isSupported("pkcs12")) {
        connect(m_loader, SIGNAL(finished()), this, SLOT(privateKeyLoaded()));
    }
}

SMIMEHelper::~SMIMEHelper()
{
}

void SMIMEHelper::privateKeyLoaded()
{
    if ( m_loader->convertResult() == QCA::ConvertGood ) {
        QCA::KeyBundle key = m_loader->keyBundle();
        QCA::SecureMessageKey skey;
        skey.setX509CertificateChain(key.certificateChain());
        skey.setX509PrivateKey(key.privateKey());
        QCA::SecureMessageKeyList skeys;
        skeys += skey;
        m_cms->setPrivateKeys(skeys);
        // trigger decryption in case the key took longer to load then the data
        handleDataChanged(QModelIndex(),QModelIndex());
    } else {
        emit decryptionFailed(tr("Unable to load S/MIME key"));
    }
}

void SMIMEHelper::decrypt(const QModelIndex &parent)
{
    if (QCA::isSupported("cms")) {
        if ( m_cms->privateKeys().size() == 0 ) {
            // TODO: make this configurable
            m_loader->loadKeyBundleFromFile("");
        }

        m_partIndex = parent;
        QModelIndex origIndex = m_partIndex.child(0, Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS);
        QModelIndex sourceIndex = origIndex.child(0, Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS);
        Q_ASSERT(sourceIndex.isValid());
        connect(sourceIndex.model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChanged(QModelIndex,QModelIndex)));
        //Trigger lazy loading of the required message parts
        sourceIndex.data(Imap::Mailbox::RolePartData);
        //call handleDataChanged at least once in case all parts are already available
        handleDataChanged(QModelIndex(),QModelIndex());
    } else
        emit decryptionFailed(qcaErrorStrings(0));
}

void SMIMEHelper::handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft)
    Q_UNUSED(bottomRight)
    QModelIndex origIndex = m_partIndex.child(0, Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS);
    QModelIndex sourceIndex = origIndex.child(0, Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS);
    if ( sourceIndex.data(Imap::Mailbox::RoleIsFetched).toBool() && m_cms->privateKeys().size() > 0 )
    {
        disconnect(sourceIndex.model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChanged(QModelIndex,QModelIndex)));

        QByteArray enc = sourceIndex.data(Imap::Mailbox::RolePartData).toByteArray();
        enc.replace("\r\n", "\n");
        QCA::Base64 dec;
        dec.setLineBreaksEnabled(true);

        QCA::SecureMessage* msg = new QCA::SecureMessage(m_cms);
        connect(msg, SIGNAL(finished()), this, SLOT(decryptionFinished()));
        msg->startDecrypt();
        msg->update(dec.decode(enc).toByteArray());
        msg->end();
    }
}

void SMIMEHelper::decryptionFinished()
{
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
#ifdef TROJITA_HAVE_MIMETIC
        mimetic::MimeEntity me(message.begin(), message.end());
        LocalMessagePart *part = mimeEntityToPart(me);
#else
        LocalMessagePart *part = new LocalMessagePart(0, "text/plain");
        part->setData(message);
#endif /* TROJITA_HAVE_MIMETIC */
        QVector<MessagePart*> children;
        children.append(part);
        emit dataDecrypted(m_partIndex, children);
    }
}

}
