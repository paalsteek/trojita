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
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"

namespace Cryptography {
OpenPGPHelper::OpenPGPHelper(QObject *parent)
    : QObject(parent)
    , m_partIndex()
    , m_pgp(this)
{

}

void OpenPGPHelper::decrypt(QModelIndex parent)
{
    qDebug() << "decrypt...";
    m_partIndex = parent;
    qDebug() << "m_partIndex" << m_partIndex << m_partIndex.data(Imap::Mailbox::RolePartMimeType);
    QModelIndex sourceIndex = m_partIndex.child(0, Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS);
    qDebug() << "sourceIndex" << sourceIndex << sourceIndex.data(Imap::Mailbox::RolePartMimeType);
    Q_ASSERT(sourceIndex.model()->rowCount(sourceIndex) == 2);
    connect(sourceIndex.model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChanged(QModelIndex,QModelIndex)));
    //Trigger lazy loading of the required message parts
    sourceIndex.child(0,0).data(Imap::Mailbox::RolePartData);
    sourceIndex.child(1,0).data(Imap::Mailbox::RolePartData);
    //call handleDataChanged at least once in case all parts are already available
    handleDataChanged(QModelIndex(),QModelIndex());
}

void OpenPGPHelper::handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft)
    Q_UNUSED(bottomRight)
    qDebug() << "handleDataChanged";
    QModelIndex sourceIndex = m_partIndex.child(0, Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS);
    if ( sourceIndex.child(0,0).data(Imap::Mailbox::RoleIsFetched).toBool() &&
         sourceIndex.child(1,0).data(Imap::Mailbox::RoleIsFetched).toBool() )
    {
        qDebug() << "data fetched";
        disconnect(sourceIndex.model(), SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChanged(QModelIndex,QModelIndex)));

        QModelIndex versionIndex = sourceIndex.child(0,0), encIndex = sourceIndex.child(1,0);
        if ( !versionIndex.data(Imap::Mailbox::RolePartData).toString().contains(QLatin1String("Version: 1")) )
        {
            //this->layout()->addWidget(new QLabel(tr("Unsupported Version"))); //TODO: this message should be more helpful
            return;
        }
        QCA::SecureMessage* msg = new QCA::SecureMessage(&m_pgp);
        qDebug() << "foobar" << msg;
        connect(msg, SIGNAL(finished()), this, SLOT(decryptionFinished()));
        msg->setFormat(QCA::SecureMessage::Ascii);
        msg->startDecrypt();
        msg->update(encIndex.data(Imap::Mailbox::RolePartData).toByteArray().data());
        msg->end();
    }
}

void OpenPGPHelper::decryptionFinished()
{
    QCA::SecureMessage* msg = qobject_cast<QCA::SecureMessage*>(sender());
    Q_ASSERT(msg);

    if (!msg->success()) {
    } else {
        QByteArray message;
        while ( msg->bytesAvailable() )
        {
            message.append(msg->read());
        }
        qDebug() << message;
        mimetic::MimeEntity *me = new mimetic::MimeEntity(message.begin(), message.end());
        emit dataDecrypted(m_partIndex, me);
    }
}

}
