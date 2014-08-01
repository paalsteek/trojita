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

#include <QDebug>
#include <QModelIndex>

#include "MessagePartFactory.h"
#include "MessageModel.h"
#include "Cryptography/OpenPGPHelper.h"
#include "Cryptography/SMIMEHelper.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"

namespace Cryptography {

MessagePartFactory::MessagePartFactory(MessageModel *model)
    : m_model(model)
    , m_pgpHelper(new Cryptography::OpenPGPHelper(this))
    , m_cmsHelper(new Cryptography::SMIMEHelper(this))
{
    connect(m_pgpHelper, SIGNAL(dataDecrypted(QModelIndex,QVector<Cryptography::MessagePart*>)), m_model, SLOT(insertSubtree(QModelIndex,QVector<Cryptography::MessagePart*>)));
    connect(m_cmsHelper, SIGNAL(dataDecrypted(QModelIndex,QVector<Cryptography::MessagePart*>)), m_model, SLOT(insertSubtree(QModelIndex,QVector<Cryptography::MessagePart*>)));
}

void MessagePartFactory::buildProxyTree(const QModelIndex& source, MessagePart *destination)
{
    int i = 0;
    QModelIndex child = source.child(i, 0);
    while ( child.isValid() ) {
        MessagePart* part = new ProxyMessagePart(destination, child, m_model);
        buildProxyTree(child, part);
        QModelIndex rawChild = child.child(0, Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS);
        if (rawChild.isValid())
            part->setRawPart(new ProxyMessagePart(part, rawChild, m_model));
        if ( child.data(Imap::Mailbox::RolePartMimeType).toString().compare("multipart/encrypted")
             && child.data(Imap::Mailbox::RolePartMimeType).toString().compare("application/pkcs7-mime") ) {
            destination->setChild(i, 0, part);
        } else {
            MessagePart* dummy = new LocalMessagePart(destination, child.data(Imap::Mailbox::RolePartMimeType).toByteArray());
            dummy->setRawPart(part);
            destination->setChild(i, 0, dummy);
        }
        child = source.child(++i, 0);
    }
}

void MessagePartFactory::buildSubtree(const QModelIndex &parent)
{
    if ( !parent.data(Imap::Mailbox::RolePartMimeType).toString().compare("multipart/encrypted") ) {
        m_pgpHelper->decrypt(parent);
    } else if ( !parent.data(Imap::Mailbox::RolePartMimeType).toString().compare("application/pkcs7-mime") ) {
        m_cmsHelper->decrypt(parent);
    } else {
        MessagePart* child = new ProxyMessagePart(0, parent, m_model);
        buildProxyTree(parent, child);
        QVector<MessagePart*> children;
        children.append(child);
        m_model->insertSubtree(QModelIndex(), children);
    }
}

}
