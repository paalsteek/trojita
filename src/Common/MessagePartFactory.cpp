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

MessagePartFactory::MessagePartFactory() : m_pgpHelper(new Cryptography::OpenPGPHelper(this)) {}

void MessagePartFactory::buildProxyTree(const QModelIndex& source, MessagePart* destination)
{
    if ( source.data(Imap::Mailbox::RolePartMimeType).toString().compare("multipart/encrypted") == 0 ) {
        //TODO: populate raw data


        // TODO: some placeholder?
        return;
    }
    int i = 0;
    QModelIndex child = source.child(i, 0);
    while ( child.isValid() ) {
        MessagePart* part = new ProxyMessagePart(destination, i, child);
        destination->setChild(i, 0, part);
        buildProxyTree(child, part);
        child = source.child(++i, 0);
    }
}

void MessagePartFactory::buildSubtree(const QModelIndex &parent, MessageModel *model)
{
    MessagePart* child = new ProxyMessagePart(0, 0, parent);
    buildProxyTree(parent, child);
    QVector<MessagePart*> children;
    children.append(child);

    model->insertSubtree(QModelIndex(), 0, 0, children);
}

}
