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

#ifndef COMMON_MODEL_H
#define COMMON_MODEL_H

#include <QHash>
#include <QIdentityProxyModel>

#include "Cryptography/OpenPGPHelper.h"
#include "Cryptography/SMIMEHelper.h"

namespace Common {

class ProxyModel: public QIdentityProxyModel
{
    Q_OBJECT

public:
    ProxyModel(QObject *parent = 0);
    ~ProxyModel();
    QVariant data(const QModelIndex &proxyIndex, int role) const;
    int rowCount(const QModelIndex &parent) const;
    static QModelIndex findProxyIndex(QModelIndex index);
    static QModelIndex findEncryptedRoot(QModelIndex index);
    QModelIndex index(int row, int column, const QModelIndex &parent) const;

    bool isNetworkOnline() const { return qobject_cast<Imap::Mailbox::Model*>(sourceModel())->isNetworkOnline(); }
    bool isNetworkAvailable() const { return qobject_cast<Imap::Mailbox::Model*>(sourceModel())->isNetworkAvailable(); }

private slots:
    void handleDecryptedData(const QModelIndex &index, mimetic::MimeEntity* me);

private:
    Cryptography::OpenPGPHelper* m_pgpHelper;
    Cryptography::SMIMEHelper* m_smimeHelper;
    mutable QHash<QString, mimetic::MimeEntity*> m_parts;
};
}
#endif /* COMMON_MODEL_H */
