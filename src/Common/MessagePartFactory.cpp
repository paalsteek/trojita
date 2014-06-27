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
#include "Imap/Model/ItemRoles.h"

namespace Common {

void MessagePartFactory::createPart(int row, int column)
{
    MessagePart *part, *child;
    part = qobject_cast<MessagePart*>(sender());
    Q_ASSERT(part);
#ifdef TROJITA_HAVE_MIMETIC
    if (column != 0) {
        EncryptedMessagePart* encPart = qobject_cast<EncryptedMessagePart*>(part);
        Q_ASSERT(encPart);
        emit newPart(row, column, encPart->rawPart());
    }
#endif /* TROJITA_HAVE_MIMETIC */
    child = part->newChild(row);

#ifdef TROJITA_HAVE_MIMETIC
    if (child && child->data(Imap::Mailbox::RolePartMimeType).isValid()
            && child->data(Imap::Mailbox::RolePartMimeType).toString().toLower() == QLatin1String("multipart/encrypted")) {
        EncryptedMessagePart* encPart = new EncryptedMessagePart(part, row, child);
        if (part) {
            part->replaceChild(row, encPart);
        }
        child = encPart;
        //connect(m_pgpHelper, SIGNAL(dataDecrypted(mimetic::MimeEntity*)), encPart, SLOT(handleDataDecrypted(mimetic::MimeEntity*)));
        //connect(encPart, SIGNAL(partDecrypted()), this, SLOT(handlePartDecrypted()));
        //m_pgpHelper->decrypt(index);
        //return index;
    }
#endif /* TROJITA_HAVE_MIMETIC */

    if (child) {
        qDebug() << "Created" << child->data(Imap::Mailbox::RolePartPathToPart).toString() << "[" << child->data(Imap::Mailbox::RolePartMimeType).toString() << "]";
        child->setObjectName("MessagePart" + child->data(Imap::Mailbox::RolePartPathToPart).toString());
    } else {
        qDebug() << "Factory found null";
        return;
    }

    emit newPart(row, column, child);
}

}
