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

#include <QModelIndex>

#include "configure.cmake.h"
#include "MessagePartFactory.h"
#include "MessageModel.h"
#include "MessagePart.h"
#ifdef TROJITA_HAVE_QCA
#include "OpenPGPHelper.h"
#endif /* TROJITA_HAVE_QCA */
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"

namespace Cryptography {

MessagePartFactory::MessagePartFactory(MessageModel *model)
    : m_model(model)
#ifdef TROJITA_HAVE_QCA
    , m_pgpHelper(new Cryptography::OpenPGPHelper(this))
{
    connect(m_pgpHelper, SIGNAL(dataDecrypted(QModelIndex,QVector<Cryptography::MessagePart*>)), m_model, SLOT(insertSubtree(QModelIndex,QVector<Cryptography::MessagePart*>)));
    connect(m_pgpHelper, SIGNAL(decryptionFailed(QString)), m_model, SIGNAL(error(QString)));
    connect(m_pgpHelper, SIGNAL(passwordRequired(int,QString)), m_model, SIGNAL(passwordRequired(int,QString)));
    connect(m_model, SIGNAL(passwordAvailable(int,QString)), m_pgpHelper, SLOT(handlePassword(int,QString)));
    connect(m_model, SIGNAL(passwordError(int)), m_pgpHelper, SLOT(handlePasswordError(int)));
}
#else
{

}
#endif /* TROJITA_HAVE_QCA */

void MessagePartFactory::buildProxyTree(const QModelIndex& source, MessagePart *destination)
{
    int i = 0;
    QModelIndex child = source.child(i, 0);
    while (child.isValid()) {
        MessagePart* part = new ProxyMessagePart(destination, child, m_model);
        buildProxyTree(child, part);
        QModelIndex rawChild = child.child(0, Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS);
        // Multipart parts do not have a raw child
        if (rawChild.isValid()) {
            part->setRawPart(new ProxyMessagePart(part, rawChild, m_model));
        }
#ifdef TROJITA_HAVE_QCA
        QString mimetype = child.data(Imap::Mailbox::RolePartMimeType).toString();
        if (mimetype == QLatin1String("multipart/encrypted")) {
            MessagePart* dummy = new LocalMessagePart(destination, mimetype.toLocal8Bit());
            dummy->setRawPart(part);
            destination->setChild(i, 0, dummy);
        } else
#endif /* TROJITA_HAVE_QCA */
        {
            destination->setChild(i, 0, part);
        }
        child = source.child(++i, 0);
    }
}

void MessagePartFactory::buildSubtree(const QModelIndex &parent)
{
#ifdef TROJITA_HAVE_QCA
    if (parent.data(Imap::Mailbox::RolePartMimeType).toString() == QLatin1String("multipart/encrypted")) {
        m_pgpHelper->decrypt(parent);
    } else
#endif /* TROJITA_HAVE_QCA */
    {
        MessagePart* child = new ProxyMessagePart(0, parent, m_model);
        buildProxyTree(parent, child);
        QVector<MessagePart*> children;
        children.append(child);
        m_model->insertSubtree(QModelIndex(), children);
    }
}

}
