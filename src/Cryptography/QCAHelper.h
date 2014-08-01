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

#ifndef CRYPTOGRAPHY_QCAHELPER_H_
#define CRYPTOGRAPHY_QCAHELPER_H_

#include <QtCrypto/QtCrypto>

namespace mimetic {
class MimeEntity;
class MailboxList;
class AddressList;
}

namespace Cryptography {
class MessagePart;
class LocalMessagePart;
}

namespace Imap {
namespace Message {
class MailAddress;
}
}

namespace Cryptography {
class QCAHelper : public QObject {
    Q_OBJECT

public:
    QCAHelper(QObject *parent);

protected slots:
    void handleEventReady(int id, const QCA::Event &e);

protected:
    static QString qcaErrorStrings(int e);
    static void storeInterestingFields(const mimetic::MimeEntity& me, LocalMessagePart* part);
    static LocalMessagePart* mimeEntityToPart(const mimetic::MimeEntity& me);
    static QList<Imap::Message::MailAddress> mailboxListToQList(const mimetic::MailboxList& list);
    static QList<Imap::Message::MailAddress> addressListToQList(const mimetic::AddressList& list);

    QCA::EventHandler _handler;
};
}

#endif /* CRYPTOGRAPHY_QCAHELPER_H_ */