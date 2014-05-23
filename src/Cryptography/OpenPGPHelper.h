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

#include <QtCrypto/QtCrypto>

//#ifdef TROJITA_HAVE_MIMETIC
#include <mimetic/mimetic.h>
//#endif // TROJITA_HAVE_MIMETIC

#include "Imap/Model/Model.h"

namespace Cryptography {

    class OpenPGPHelper : public QObject {
        Q_OBJECT

    public:
        OpenPGPHelper(QObject* parent);
        ~OpenPGPHelper() {}
        void decrypt(const QModelIndex& parent);

    signals:
        void dataDecrypted(mimetic::MimeEntity *pMe);
        void decryptionFailed(const QString& error);

    private slots:
        void handleDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);
        void decryptionFinished();

    private:
        QModelIndex m_partIndex;
        QCA::OpenPGP m_pgp;

        static QString qcaErrorStrings(int e) {
            QMap<int, QString> map;
            map[QCA::SecureMessage::ErrorPassphrase] =	tr("passphrase was either wrong or not provided");
            map[QCA::SecureMessage::ErrorFormat] = tr("input format was bad");
            map[QCA::SecureMessage::ErrorSignerExpired] = tr("signing key is expired");
            map[QCA::SecureMessage::ErrorSignerInvalid] = tr("signing key is invalid in some way");
            map[QCA::SecureMessage::ErrorEncryptExpired] = tr("encrypting key is expired");
            map[QCA::SecureMessage::ErrorEncryptUntrusted] = tr("encrypting key is untrusted");
            map[QCA::SecureMessage::ErrorEncryptInvalid] = tr("encrypting key is invalid in some way");
            map[QCA::SecureMessage::ErrorNeedCard] = tr("pgp card is missing");
            map[QCA::SecureMessage::ErrorCertKeyMismatch] = tr("certificate and private key don't match");
            map[QCA::SecureMessage::ErrorUnknown] = tr("other error");
            return map[e];
        }
    };
}

#endif /* CRYPTOGRAPHY_OPENPGPHELPER_H_ */
