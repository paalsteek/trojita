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
#include "ProxyModel.h"
#include "Cryptography/OpenPGPHelper.h"
#include "Imap/Model/ItemRoles.h"

namespace Common {

MessagePartFactory::MessagePartFactory(MessageModel *model)
    : m_model(model)
    , m_pgpHelper(new Cryptography::OpenPGPHelper(this))
{
    connect(m_pgpHelper, SIGNAL(dataDecrypted(QModelIndex,QVector<Common::MessagePart*>)), m_model, SLOT(insertSubtree(QModelIndex,QVector<Common::MessagePart*>)));
}

void MessagePartFactory::buildProxyTree(const QModelIndex& source, MessagePart *destination)
{
    int i = 0;
    QModelIndex child = source.child(i, 0);
    while ( child.isValid() ) {
        MessagePart* part = new ProxyMessagePart(destination, child);
        buildProxyTree(child, part);
        if ( child.data(Imap::Mailbox::RolePartMimeType).toString().compare("multipart/encrypted") ) {
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
    if (parent.data(Imap::Mailbox::RolePartMimeType).toString().compare("multipart/encrypted") ) {
        MessagePart* child = new ProxyMessagePart(0, parent);
        buildProxyTree(parent, child);
        QVector<MessagePart*> children;
        children.append(child);
        m_model->insertSubtree(QModelIndex(), children);
    } else {
        m_pgpHelper->decrypt(parent);
    }
}

}
