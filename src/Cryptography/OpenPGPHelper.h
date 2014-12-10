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

#ifndef CRYPTOGRAPHY_OPENPGPHELPER_H_
#define CRYPTOGRAPHY_OPENPGPHELPER_H_

#include <QModelIndex>
#include "QCAHelper.h"

namespace QCA {
class OpenPGP;
}

namespace Cryptography {

/** @short Wrapper for asynchronous PGP related operations using QCA */
class OpenPGPHelper : public QCAHelper {
    Q_OBJECT

public:
    OpenPGPHelper(QObject* parent);
    ~OpenPGPHelper();
    void decrypt(const QModelIndex& parent);

signals:
    void dataDecrypted(const QModelIndex& parent, const QVector<Cryptography::MessagePart*>& part);
    void decryptionFailed(const QString& error);

private slots:
    void handleDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);
    void decryptionFinished();

private:
    QModelIndex m_partIndex;
    QCA::OpenPGP *m_pgp;
};
}

#endif /* CRYPTOGRAPHY_OPENPGPHELPER_H_ */