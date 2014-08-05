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

#include <QTest>
#include <QNetworkRequest>
#include <QNetworkReply>

#include "Imap/data.h"
#include "test_Cryptography_MessageModel.h"
#include "Cryptography/MessageModel.h"
#include "Streams/FakeSocket.h"
#include "Utils/headless_test.h"
#include "Imap/Model/ItemRoles.h"

void CryptographyMessageModelTest::testMessageParts()
{
    QFETCH(QByteArray, bodystructure);
    QFETCH(QByteArray, partId);
    QFETCH(pathList, path);
    QFETCH(QByteArray, data);

    // By default, there's a 50ms delay between the time we request a part download and the time it actually happens.
    // That's too long for a unit test.
    model->setProperty("trojita-imap-delayed-fetch-part", 0);

    helperSyncBNoMessages();
    cServer("* 1 EXISTS\r\n");
    cClient(t.mk("UID FETCH 1:* (FLAGS)\r\n"));
    cServer("* 1 FETCH (UID 333 FLAGS ())\r\n" + t.last("OK fetched\r\n"));

    QCOMPARE(model->rowCount(msgListB), 1);
    QModelIndex msg = msgListB.child(0, 0);
    QVERIFY(msg.isValid());
    QCOMPARE(model->rowCount(msg), 0);
    cClient(t.mk("UID FETCH 333 (" FETCH_METADATA_ITEMS ")\r\n"));
    cServer("* 1 FETCH (UID 333 BODYSTRUCTURE (" + bodystructure + "))\r\n" + t.last("OK fetched\r\n"));
    cEmpty();
    QVERIFY(model->rowCount(msg) > 0);
    Cryptography::MessageModel msgModel(0, msg);
    QModelIndex mappedMsg = msgModel.index(0,0);
    QVERIFY(mappedMsg.isValid());
    QVERIFY(msgModel.rowCount(mappedMsg) > 0);

    QModelIndex mappedPart = mappedMsg;
    for (QList<QPair<int,int> >::iterator it = path.begin(), end = path.end(); it != end; ++it) {
        mappedPart = mappedPart.child(it->first, it->second);
    }
    QCOMPARE(mappedPart.data(Imap::Mailbox::RolePartData).toString(), QString());
    cClient(t.mk("UID FETCH 333 (BODY.PEEK[") + partId + "])\r\n");
    cServer("* 1 FETCH (UID 333 BODY[" + partId + "] {" + QByteArray::number(data.size()) + "}\r\n" +
            data + ")\r\n" + t.last("OK fetched\r\n"));
    cEmpty();
    QCOMPARE(mappedPart.data(Imap::Mailbox::RolePartData).toByteArray(), data);
    QVERIFY(errorSpy->isEmpty());
}

void CryptographyMessageModelTest::testMessageParts_data()
{
    QTest::addColumn<QByteArray>("bodystructure");
    QTest::addColumn<QByteArray>("partId");
    QTest::addColumn<QList<QPair<int,int> > >("path");
    QTest::addColumn<QByteArray>("data");

    QTest::newRow("plaintext/root")
            << bsPlaintext
            << QByteArray("1")
            << (QList<QPair<int,int> >() << QPair<int,int>(0,0))
            << QByteArray("blesmrt");

    QTest::newRow("torture/plaintext")
            << bsTortureTest
            << QByteArray("1")
            << (pathList() << QPair<int,int>(0,0) << QPair<int,int>(0,0))
            << QByteArray("plaintext");

    QTest::newRow("torture/richtext")
            << bsTortureTest
            << QByteArray("2.2.1")
            << (pathList() << QPair<int,int>(0,0) << QPair<int,int>(1,0)
                << QPair<int,int>(0,0) << QPair<int,int>(1,0) << QPair<int,int>(0,0))
            << QByteArray("text richtext");
}

TROJITA_HEADLESS_TEST( CryptographyMessageModelTest )
