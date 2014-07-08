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
#ifndef KEYLISTMODEL_H
#define KEYLISTMODEL_H

#include <QAbstractListModel>

#include <QtCrypto/QtCrypto>

class KeyListModel : public QAbstractListModel
{
    Q_OBJECT
public:
    explicit KeyListModel(QObject *parent = 0);
    virtual int rowCount(const QModelIndex &parent) const;
    virtual QVariant data(const QModelIndex &index, int role) const;

signals:

public slots:

private:
    QCA::KeyStoreManager m_manager;
    QCA::KeyStore *m_store;
    QList<QCA::PGPKey> m_list;
};

#endif // KEYLISTMODEL_H
