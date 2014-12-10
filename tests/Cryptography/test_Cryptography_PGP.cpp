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

#include <QtCrypto/QtCrypto>
#include <QtTest/QtTest>

#include "test_Cryptography_PGP.h"
#include "Cryptography/MessageModel.h"
#include "Imap/data.h"
#include "Imap/Model/ItemRoles.h"
#include "Streams/FakeSocket.h"
#include "Utils/headless_test.h"

/* TODO: test cases:
 *   * everything fine
 *   * decrypt corrupt data
 *   * decrypt with expired key
 *   * decrypt with not yet valid key
 *   * decrypt without matching key
 *   * verify valid signature
 *   * verify corrupted signature
 *   * verify signature of modified text
 *   * verify signature with expired key
 *   * verify signature with not yet valid key
 *   * verify signature with unkown key
 *   * nested scenarios
 */

void CryptographyPGPTest::testDecryption()
{
    QFETCH(QByteArray, bodystructure);
    QFETCH(QByteArray, cyphertext);
    QFETCH(QByteArray, plaintext);
    QFETCH(bool, successful);

    QByteArray oldGNUPGHOME = qgetenv("GNUPGHOME");
    if (!qputenv("GNUPGHOME", "./keys")) {
        QFAIL("Unable to set GNUPGHOME environment variable");
    }

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

    QModelIndex data = mappedMsg.child(0, 0);
    QVERIFY(data.isValid());
    QCOMPARE(msgModel.rowCount(data), 0);
    QCOMPARE(data.data(Imap::Mailbox::RoleIsFetched).toBool(), false);

    cClientRegExp(t.mk("UID FETCH 333 \\((BODY\\.PEEK\\[2\\] BODY\\.PEEK\\[1\\]|BODY.PEEK\\[1\\] BODY\\.PEEK\\[2\\])\\)"));
    cServer("* 1 FETCH (UID 333 BODY[2] {" + QByteArray::number(cyphertext.size())
            + "}\r\n" + cyphertext + " BODY[1] {12}\r\nVersion: 1\r\n)\r\n"
            + t.last("OK fetched"));

    QSignalSpy qcaSuccessSpy(&msgModel, SIGNAL(rowsInserted(const QModelIndex &,int,int)));
    QSignalSpy qcaErrorSpy(&msgModel, SIGNAL(error(QString)));

    int i = 0;
    while (qcaSuccessSpy.empty() && qcaErrorSpy.empty() && i < 50) {
        QTest::qWait(250);
    }

    QCOMPARE(qcaErrorSpy.empty(), successful);
    QCOMPARE(qcaSuccessSpy.empty(), !successful);

    QVERIFY(data.data(Imap::Mailbox::RoleIsFetched).toBool() == successful);

    cEmpty();
    QVERIFY(errorSpy->empty());

    if (!oldGNUPGHOME.isNull()) {
        qputenv("GNUPGHOME", oldGNUPGHOME);
    }
}

void CryptographyPGPTest::testDecryption_data()
{
    QTest::addColumn<QByteArray>("bodystructure");
    QTest::addColumn<QByteArray>("cyphertext");
    QTest::addColumn<QByteArray>("plaintext");
    QTest::addColumn<bool>("successful");

    QTest::newRow("valid")
            << QByteArray("(\"application\" \"pgp-encrypted\" NIL NIL NIL \"7bit\" 12 NIL NIL NIL NIL)(\"application\" \"octet-stream\" (\"name\" \"encrypted.asc\") NIL \"OpenPGP encrypted message\" \"7bit\" 4127 NIL (\"inline\" (\"filename\" \"encrypted.asc\")) NIL NIL) \"encrypted\" (\"protocol\" \"application/pgp-encrypted\" \"boundary\" \"trojita=_7cf0b2b6-64c6-41ad-b381-853caf492c54\") NIL NIL NIL")
            << QByteArray("-----BEGIN PGP MESSAGE-----\r\n"
                          "Version: GnuPG v2\r\n"
                          "hQEMA/v9vPJhwIfsAQgAiuYNDjmFIWWSLMaesVqSCzGLestByq8jMPmOBWGzARPH\r\n"
                          "lxJwBvPBk0ipJrphs/Sg3KHkwa/uq4rukpmvUUwGMshpEf1qSWaK3xPo7VS44kBB\r\n"
                          "WhFU18tgpRN5cG1/shc69UnZbnvhqI6/cZLsvEHk03uBog2JVsJpmWB9Sdrmmqw8\r\n"
                          "QxA+T3AT6rWXBievJTLgYK4Ol9O+6OY0K/bxnE32NR38CNLa9/fN+gSFxDE9B1pp\r\n"
                          "Ch51Km35vQg++8tPieNQdGag7Jnf1CBGMLRADFOE+ERP9nV4squhkI93sUEUMgpD\r\n"
                          "boP9Kx7MzF5bbr0cRFbs0V9MDbzBk2Yfw8nBbA9Wr9JFATr1YAVdXdsCdHIQc8vo\r\n"
                          "VwJxnKW0CjHeZJljZJHexArkTekzYIWBuR5J2bhyVEZXrPXuFcfobiO2MBeYu5lO\r\n"
                          "0qCPsucs\r\n"
                          "=nulP\r\n"
                          "-----END PGP MESSAGE-----\r\n")
            << QByteArray("plaintext")
            << true;
    QTest::newRow("invalid")
            << QByteArray("(\"application\" \"pgp-encrypted\" NIL NIL NIL \"7bit\" 12 NIL NIL NIL NIL)(\"application\" \"octet-stream\" (\"name\" \"encrypted.asc\") NIL \"OpenPGP encrypted message\" \"7bit\" 4127 NIL (\"inline\" (\"filename\" \"encrypted.asc\")) NIL NIL) \"encrypted\" (\"protocol\" \"application/pgp-encrypted\" \"boundary\" \"trojita=_7cf0b2b6-64c6-41ad-b381-853caf492c54\") NIL NIL NIL")
            << QByteArray("-----BEGIN PGP MESSAGE-----\r\n"
                          "Version: GnuPG v2\r\n"
                          "hQEMA/v9vPJhwIfsAQgAiuYNDjmFIWWSLMaesVqSCzGLestByq8jMPmOBWGzARPH\r\n"
                          "lxJwBvPBk0ipJrphs/Sg3KHkwa/uq4rukpmvUUwGMshpEf1qSWaK3xPo7VS44kBB\r\n"
                          "WhFU18tgpRN5cG1/shc69UnZbnvhqI6/cZLsvEHk03uBog2JVsJpmWB9Sdrmmqw8\r\n"
                          "QxA+T3AT6rWXBievJTLgYK4Ol91+6OY0K/bxnE32NR38CNLa9/fN+gSFxDE9B1pp\r\n"
                          "Ch51Km35vQg++8tPieNQdGag7Jnf1CBGMLRADFOE+ERP9nV4squhkI93sUEUMgpD\r\n"
                          "boP9Kx7MzF5bbr0cRFbs0V9MDbzBk2Yfw8nBbA9Wr9JFATr1YAVdXdsCdHIQc8vo\r\n"
                          "VwJxnKW0CjHeZJljZJHexArkTekzYIWBuR5J2bhyVEZXrPXuFcfobiO2MBeYu5lO\r\n"
                          "0qCPsucs\r\n"
                          "=nulP\r\n"
                          "-----END PGP MESSAGE-----\r\n")
            << QByteArray("plaintext")
            << false;
    QTest::newRow("expiredKey")
            << QByteArray("(\"application\" \"pgp-encrypted\" NIL NIL NIL \"7bit\" 12 NIL NIL NIL NIL)(\"application\" \"octet-stream\" (\"name\" \"encrypted.asc\") NIL \"OpenPGP encrypted message\" \"7bit\" 4127 NIL (\"inline\" (\"filename\" \"encrypted.asc\")) NIL NIL) \"encrypted\" (\"protocol\" \"application/pgp-encrypted\" \"boundary\" \"trojita=_7cf0b2b6-64c6-41ad-b381-853caf492c54\") NIL NIL NIL")
            << QByteArray("-----BEGIN PGP MESSAGE-----\r\n"
                          "Version: GnuPG v2\r\n\r\n"
                          "hQEMA4gswqb3049hAQf+PdL+UaZWLIkfWD0VODhnd6lv8YfRwc6FYWKV8EmXjlaV\r\n"
                          "JfdhBHW0Y2V4AuzwX135ZA29sD50p6+QwVoX7FHCKFUc/rH/+eOHGBockm17gxLY\r\n"
                          "S3KnNK8bKhH/10NxpKLA8ESe4oOyg3gcR0HjLragFEGF3YPT2xpgnV5uYcde7dbp\r\n"
                          "bqdGgAH4x9JP7EqjXetLfpwcv0ERAtZt/4r9vVthDas9RjV/8vOOqYto/wCLjIR2\r\n"
                          "NB50xsPEn1wQdKylVdl4m59R7dMMw//eiJR1Zklm2glukrusFig/v7ZBIM7bq/Ir\r\n"
                          "Lhb4Sdsb6xvvww1RdMZoa9aBNqEUElMRNJWgJjJxQtJFASJoXtSY3hNlch8/Faf2\r\n"
                          "nfROWp2Gi3QRo6An1FtHaqRzhN3QodOjxrdKG0+5KVEl/482uZ/vHxlto7DfLCRi\r\n"
                          "jWNlQum6\r\n"
                          "=JXza\r\n"
                          "-----END PGP MESSAGE-----\r\n")
            << QByteArray("plaintext")
            << true;
    QTest::newRow("unknownKey")
            << QByteArray("(\"application\" \"pgp-encrypted\" NIL NIL NIL \"7bit\" 12 NIL NIL NIL NIL)(\"application\" \"octet-stream\" (\"name\" \"encrypted.asc\") NIL \"OpenPGP encrypted message\" \"7bit\" 4127 NIL (\"inline\" (\"filename\" \"encrypted.asc\")) NIL NIL) \"encrypted\" (\"protocol\" \"application/pgp-encrypted\" \"boundary\" \"trojita=_7cf0b2b6-64c6-41ad-b381-853caf492c54\") NIL NIL NIL")
            << QByteArray("-----BEGIN PGP MESSAGE-----\r\n"
                          "Version: GnuPG v2\r\n\r\n"
                          "hQIMAwAAAAAAAAAAAQ/6ApqIv2UBcwzdyOhaqyI7MSKd7gSejnR8AjeIlrdZVHhH\r\n"
                          "UBn3DkaZO76nV8eRmXtkQamAP9FYhDpFm3yOb5vi6nVu2XTTRv/RTrNzWTFsLzRG\r\n"
                          "jvTIVZ/GzmJHU1NtDBccEY2eDSAMW98nrjRQTYHI5WO0WW1hlwjVYdyUyG2bSXwn\r\n"
                          "3YP2rLgYc6JrMQKEcJIXEb/LqCLgJJ86kRxJ83gsb5y2MUdzq2EqKG6s5C2/0cPc\r\n"
                          "Uk9UoraBcMMZgYcFqvAG5euE56n1JCYgXQ6PesOEQj/kYAoV0YNyagH/jO43rj3S\r\n"
                          "fFTG8t7KjluqVrgauKCei3M7eq5RYmRdSGI/jbQBR98ZdnZxTW1aE2SKpzV8P03o\r\n"
                          "KtYFG8WADaYILn4hJGfY6FUbEpqJF2Glv8aQ3LZ/G8J+kMpNTcFerSbw6FGKZfFb\r\n"
                          "Fffz44lnG1OUfNeSWVeHzJqlCxTiaRaN2ZGorCs56dR64IIYZHE8nIFhQH4/XLeL\r\n"
                          "vwHCnu0uJ2TB578hBCm0N8BzWM2ZONwASqNsHBcMZsDC5SWphLkzCxlDDmTPQOKr\r\n"
                          "ujLuJ7lcjVeXvsskfa0i9hT3mAmNh9TdQbBIJb9CQz/0/rvPSzxJqUBhMxuxotgr\r\n"
                          "Sy+1nfqhO0yeUmXXNiGyUFtxNpCXf6e/ziz3dkPtAF52DkfMK7uhchuGYx86IWjS\r\n"
                          "RQF717OVGoHh37FzWwuftDfu6cKb3tI/7aENiqIjSe81I7JtgqHZYKXbKtmEtnOP\r\n"
                          "bnuj7O1VeOHFmobFZA7ymtLYKXaM8A==\r\n"
                          "=12Q9\r\n"
                          "-----END PGP MESSAGE-----\r\n")
            << QByteArray("plaintext")
            << false;
}

TROJITA_HEADLESS_TEST( CryptographyPGPTest )
