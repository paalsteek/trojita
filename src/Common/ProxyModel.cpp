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

#include "ProxyModel.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/Model.h"
#include "Imap/Model/MailboxTree.h"
#include <QDebug>

namespace Common {

//#ifdef TROJITA_HAVE_GNUPG
QCA::Initializer init;
//#endif // TROJITA_HAVE_GNUPG

MessagePart::MessagePart(QModelIndex sourceIndex, MessagePart *parent)
    : m_parent(parent)
    , m_children()
    , m_me(0)
    , m_sourceIndex(sourceIndex)
{
}

MessagePart::MessagePart(mimetic::MimeEntity *pMe, MessagePart *parent)
    :m_parent(parent)
    , m_children()
    , m_me(pMe)
    , m_sourceIndex()
{

}

MessagePart::~MessagePart()
{
    Q_FOREACH(MessagePart* part, m_children)
    {
        delete part;
    }
}

int MessagePart::findRow(MessagePart *child)
{
    for (int i = 0; i < m_children.size(); ++i)
    {
        if (m_children[i] == child)
            return i;
    }

    return -1;
}

QVariant MessagePart::mimetype()
{
    if (m_me) {
        QString type = QString::fromStdString(m_me->header().contentType().type());
        QString subtype = QString::fromStdString(m_me->header().contentType().subtype());

        return QString("%1/%2").arg(type, subtype);
    } else if (m_sourceIndex.isValid()) {
        return m_sourceIndex.data(Imap::Mailbox::RolePartMimeType);
    }

    return QVariant();
}

MessageModel::MessageModel(QModelIndex message, QObject *parent)
    : QAbstractItemModel(parent)
    , m_message(message)
{
}

MessageModel::~MessageModel()
{
}

QModelIndex MessageModel::index(int row, int column, const QModelIndex &parent) const
{
    if (!parent.isValid())
        return createIndex(row, column, new MessagePart(m_message.child(0,0)));

    MessagePart* part = static_cast<MessagePart*>(parent.internalPointer());
    return createIndex(row, column, part->child(row));
}

QModelIndex MessageModel::parent(const QModelIndex &child) const
{
    if (!child.isValid())
        return QModelIndex();

    MessagePart* part = static_cast<MessagePart*>(child.internalPointer());
    if (!part->parent())
        return QModelIndex();

    if (int row = part->parent()->findRow(part) >= 0)
        return createIndex(row, 0, part->parent());

    return QModelIndex();
}

int MessageModel::rowCount(const QModelIndex &parent) const
{
    MessagePart* part = static_cast<MessagePart*>(parent.internalPointer());
    return part->rowCount();
}

QVariant MessageModel::data(const QModelIndex &index, int role) const
{
    MessagePart* part = static_cast<MessagePart*>(index.internalPointer());
    switch (role) {
    case Imap::Mailbox::RolePartData:
        break;
    case Imap::Mailbox::RolePartMimeType:
        return part->mimetype();
    case Imap::Mailbox::RolePartCharset:
    case Imap::Mailbox::RolePartContentFormat:
    case Imap::Mailbox::RolePartContentDelSp:
    case Imap::Mailbox::RolePartEncoding:
    case Imap::Mailbox::RolePartBodyFldId:
    case Imap::Mailbox::RolePartBodyDisposition:
    case Imap::Mailbox::RolePartFileName:
    case Imap::Mailbox::RolePartOctets:
    case Imap::Mailbox::RolePartId:
    case Imap::Mailbox::RolePartPathToPart:
    case Imap::Mailbox::RolePartMultipartRelatedMainCid:
    case Imap::Mailbox::RolePartIsTopLevelMultipart:
    default:
        break;
    }

    return QVariant();
}

#if 0
ProxyModel::ProxyModel(QObject *parent)
    : QIdentityProxyModel(parent)
    , m_pgpHelper(new Cryptography::OpenPGPHelper(this))
    , m_smimeHelper(new Cryptography::SMIMEHelper(this))
    , m_parts()
{
}

ProxyModel::~ProxyModel()
{
}

int ProxyModel::rowCount(const QModelIndex &parent) const
{
    //TODO: if (dynamic_cast<...>(parent.internalPointer())) { ... }

    try {
        if (typeid(*((mimetic::MimeEntity*) parent.internalPointer())) == typeid(mimetic::MimeEntity)) {
            mimetic::MimeEntity* me = static_cast<mimetic::MimeEntity*>(parent.internalPointer());
            return me->body().parts().size();
        }
    } catch (const std::bad_typeid& e) {

    }

    if (parent.data(Imap::Mailbox::RolePartMimeType) == "multipart/encrypted") {
        qDebug() << "encrypted has children?";

        QString id = sourceModel()->data(mapToSource(parent), Imap::Mailbox::RolePartId).toString();
        if (parent.data(Imap::Mailbox::RolePartIsTopLevelMultipart).toBool()) {
            Imap::Mailbox::TreeItem* grandParent = static_cast<Imap::Mailbox::TreeItem*>(parent.parent().internalPointer());
            if (Imap::Mailbox::TreeItemMessage* msg = dynamic_cast<Imap::Mailbox::TreeItemMessage*>(grandParent))
                id = msg->uid();
        }
        qDebug() << "part" << id << "in" << m_parts.keys() << "?";
        QHash<QString, mimetic::MimeEntity*>::iterator it = m_parts.find(id);
        if (it == m_parts.end()) {
            qDebug() << "not yet decrypted";
            m_parts.insert(id, 0);
            connect(m_pgpHelper, SIGNAL(dataDecrypted(QModelIndex,mimetic::MimeEntity*)), this, SLOT(handleDecryptedData(QModelIndex,mimetic::MimeEntity*)));
            m_pgpHelper->decrypt(parent);
        } else {
            qDebug() << "decryption in progress";
            if (it.value()) {
                qDebug() << "decryption finished";
                qDebug() << "result" << it.value()->body().parts().size();
                return it.value()->body().parts().size();
            }
        }
        //return parsedData.rowcount
        return 0;
    }

    return sourceModel()->rowCount(mapToSource(parent));
}

QVariant ProxyModel::data(const QModelIndex &proxyIndex, int role) const
{
    if (!proxyIndex.isValid())
        return QVariant();

    try {
        if (typeid(*((mimetic::MimeEntity*) proxyIndex.internalPointer())) == typeid(mimetic::MimeEntity)) {
            mimetic::MimeEntity *me = static_cast<mimetic::MimeEntity*>(proxyIndex.internalPointer());
            switch (role) {
            case Imap::Mailbox::RolePartMimeType:
            {
                std::string mimeType = me->header().contentType().type() + "/" + me->header().contentType().subtype();
                return QString::fromStdString(mimeType);
            }
            case Imap::Mailbox::RolePartPathToPart:
            {
                //return QString::fromUtf8("%1/%2").arg(proxyIndex.parent().data(Imap::Mailbox::RolePartPathToPart).toString(), QString::number(proxyIndex.row()));
            }
            case Imap::Mailbox::RolePartBodyDisposition:
            case Imap::Mailbox::RolePartBodyFldId:
            case Imap::Mailbox::RolePartCharset:
            case Imap::Mailbox::RolePartContentDelSp:
            case Imap::Mailbox::RolePartData:
            default:
                return QVariant();
            }
        }
    } catch (const std::bad_typeid& e) {
        //this should not happen!
    }

    if (mapToSource(proxyIndex).data(Imap::Mailbox::RolePartMimeType).toString() == QLatin1String("multipart/encrypted")) {
        switch (role) {
        case Imap::Mailbox::RolePartData:
            qDebug() << "encrypted PartData requested";
            return QVariant();
        case Imap::Mailbox::RoleIsFetched:
            return false;
        }

        //return QVariant();
    }
    return sourceModel()->data(mapToSource(proxyIndex), role);
}

QModelIndex ProxyModel::index(int row, int column, const QModelIndex &parent) const
{
    qDebug() << "index:" << row << column << parent.data(Imap::Mailbox::RolePartMimeType);
    try {
        qDebug() << "typeId(parent.internalPointer()) =" << typeid(*((mimetic::MimeEntity*) parent.internalPointer())).name();
        if (typeid(*((mimetic::MimeEntity*) parent.internalPointer())) == typeid(mimetic::MimeEntity)) {
            mimetic::MimeEntity *me = static_cast<mimetic::MimeEntity*>(parent.internalPointer());
            Q_ASSERT(row >= 0);
            unsigned int urow = row;
            if (me->body().parts().size() > urow) {
                mimetic::MimeEntityList::iterator it = me->body().parts().begin();
                for (int i = 0; i < row; ++i)
                    it++;

                return createIndex(row, column, *it); // create TreeItems here
                /*
                 * This will simplify the distinction whether we have a local index or a remote one (TreeItem as base class)
                 * and we would have to modify realTreeItem otherwise.
                 */
            } else {
                return QModelIndex();
            }
        }
    } catch (const std::bad_typeid& e) {
        //This should never happen!
    }

    /*mimetic::MimeEntity *me = dynamic_cast<mimetic::MimeEntity*>(parent.internalPointer());
    if (me)
    {
        if (me->body().parts().size() > row) {
            return createIndex(row, column, me->body().parts()[row]);
        } else {
            return QModelIndex();
        }
    }*/
    const QModelIndex sourceParent = mapToSource(parent);
    if (parent.data(Imap::Mailbox::RolePartMimeType).toString() == "multipart/encrypted") {
        if (column == Imap::Mailbox::TreeItem::OFFSET_RAW_CONTENTS) {
            return sourceParent;
        } else {
            QString id = sourceModel()->data(mapToSource(parent), Imap::Mailbox::RolePartId).toString();
            if (parent.data(Imap::Mailbox::RolePartIsTopLevelMultipart).toBool()) {
                Imap::Mailbox::TreeItem* grandParent = static_cast<Imap::Mailbox::TreeItem*>(parent.parent().internalPointer());
                if (Imap::Mailbox::TreeItemMessage* msg = dynamic_cast<Imap::Mailbox::TreeItemMessage*>(grandParent))
                    id = msg->uid();
            }

            if (m_parts.find(id) == m_parts.end()) {
                return QModelIndex();
            }

            qDebug() << "createIndex...";
            return createIndex(row, column, m_parts[id]);
        }
    }
    /*if (!hasIndex(row, column, parent))
        return QModelIndex();*/
    qDebug() << "sourceParent" << sourceParent;
    const QModelIndex sourceIndex = sourceModel()->index(row, column, sourceParent);
    if(sourceIndex.isValid())
        return mapFromSource(sourceIndex);
    return QModelIndex();
}

void ProxyModel::handleDecryptedData(const QModelIndex& index, mimetic::MimeEntity *me)
{
    QString id = sourceModel()->data(mapToSource(index), Imap::Mailbox::RolePartId).toString();
    if (index.data(Imap::Mailbox::RolePartIsTopLevelMultipart).toBool()) {
        Imap::Mailbox::TreeItem* grandParent = static_cast<Imap::Mailbox::TreeItem*>(index.parent().internalPointer());
        if (Imap::Mailbox::TreeItemMessage* msg = dynamic_cast<Imap::Mailbox::TreeItemMessage*>(grandParent))
            id = msg->uid();
    }
    qDebug() << "handleDecryptedData" << id;
    m_parts.remove(id);
    m_parts.insert(id, me);
    beginInsertRows(index, 0, me->body().parts().size()); // have to call this after the modification as otherwise rowCount is called before we updated m_parts
    endInsertRows();
    emit dataChanged(index, index);
}

QModelIndex ProxyModel::findProxyIndex(QModelIndex index)
{
    QAbstractItemModel *model = const_cast<QAbstractItemModel*>(index.model());
    while (QAbstractProxyModel *proxy = qobject_cast<QAbstractProxyModel*>(model)) {
        Common::ProxyModel *proxyModel = qobject_cast<Common::ProxyModel*>(proxy);
        if (proxyModel) {
            return index;
        } else {
            index = proxy->mapToSource(index);
            model = proxy->sourceModel();
        }
    }
    return QModelIndex();
}

QModelIndex ProxyModel::findEncryptedRoot(QModelIndex index)
{
    while (dynamic_cast<Imap::Mailbox::TreeItemPart*>(Imap::Mailbox::Model::realTreeItem(index))) {
        if (index.data(Imap::Mailbox::RolePartMimeType).toString() == QLatin1String("multipart/encrypted")) {
            return index;
        } else {
            index = index.parent();
        }
    }

    return QModelIndex();
}
#endif
}
