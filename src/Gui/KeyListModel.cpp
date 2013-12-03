/* Copyright (C) 2013 Stephan Platz <trojita@paalsteek.de>

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
#include "KeyListModel.h"

//#include <QtConcurrent/QtConcurrentMap>

QCA::PGPKey pgpPublicKey(const QCA::KeyStoreEntry &e)
{
    return e.pgpPublicKey();
}

KeyListModel::KeyListModel(QObject *parent) :
    QAbstractListModel(parent), m_list()
{
    m_manager.start();
    m_manager.waitForBusyFinished(); //TODO: synchronous wait?
    m_store = new QCA::KeyStore("qca-gnupg", &m_manager);

    //m_list = QtConcurrent::blockingMapped(m_store->entryList(), pgpPublicKey);
    Q_FOREACH(QCA::KeyStoreEntry e, m_store->entryList())
    {
        if (!e.pgpSecretKey().isNull())
            m_list.append(e.pgpSecretKey());
    }
}

int KeyListModel::rowCount(const QModelIndex &parent) const
{
    return m_list.size();
}

QVariant KeyListModel::data(const QModelIndex &index, int role) const
{
    switch(role)
    {
    case Qt::DisplayRole:
        return QVariant(QString("%1 (%2)").arg(m_list[index.row()].primaryUserId(), m_list[index.row()].keyId()));
    }
    return QVariant();
}
