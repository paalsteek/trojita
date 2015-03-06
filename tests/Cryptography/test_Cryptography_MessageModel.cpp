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
#include <QStandardItemModel>

#include "Imap/data.h"
#include "test_Cryptography_MessageModel.h"
#include "Cryptography/MessageModel.h"
#include "Cryptography/MessagePart.h"
#include "Streams/FakeSocket.h"
#include "Utils/headless_test.h"
#include "Imap/Model/ItemRoles.h"

class MessageTestTree {
public:
    MessageTestTree(const QByteArray& mimeType) : m_mimeType(mimeType) {}
    ~MessageTestTree() { Q_FOREACH(MessageTestTree* t, m_children) { delete t; } }
    void addChild(MessageTestTree *child) { m_children.append(child); }
    int childCount() { return m_children.size(); }
    QByteArray mimeType() { return m_mimeType; }
private:
    QList<MessageTestTree*> m_children;
    QByteArray m_mimeType;
};

class MessageTestTreeLeaf : public MessageTestTree {
public:
    MessageTestTreeLeaf(const QByteArray& mimeType, const QByteArray& data) : MessageTestTree(mimeType), m_data(data) {}
    void addChild(MessageTestTree *child) {}
    int childCount() { return 0; }
private:
    QByteArray m_data;
};

void CryptographyMessageModelTest::testImapMessageParts()
{
    QFETCH(QByteArray, bodystructure);
    QFETCH(QByteArray, partId);
    QFETCH(pathList, path);
    QFETCH(QByteArray, pathToPart);
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
        QVERIFY(mappedPart.isValid());
    }
    QCOMPARE(mappedPart.data(Imap::Mailbox::RolePartData).toString(), QString());
    cClient(t.mk("UID FETCH 333 (BODY.PEEK[" + partId + "])\r\n"));
    cServer("* 1 FETCH (UID 333 BODY[" + partId + "] {" + QByteArray::number(data.size()) + "}\r\n" +
            data + ")\r\n" + t.last("OK fetched\r\n"));

    QCOMPARE(mappedPart.data(Imap::Mailbox::RolePartData).toByteArray(), data);
    QCOMPARE(mappedPart.data(Imap::Mailbox::RolePartPathToPart).toByteArray(), pathToPart);

    cEmpty();
    QVERIFY(errorSpy->isEmpty());
}

void CryptographyMessageModelTest::testImapMessageParts_data()
{
    QTest::addColumn<QByteArray>("bodystructure");
    QTest::addColumn<QByteArray>("partId");
    QTest::addColumn<QList<QPair<int,int> > >("path");
    QTest::addColumn<QByteArray>("pathToPart");
    QTest::addColumn<QByteArray>("data");

    QTest::newRow("plaintext-root")
            << bsPlaintext
            << QByteArray("1")
            << (pathList() << qMakePair(0,0))
            << QByteArray("/0")
            << QByteArray("blesmrt");

    QTest::newRow("torture-plaintext")
            << bsTortureTest
            << QByteArray("1")
            << (pathList() << qMakePair(0,0) << qMakePair(0,0))
            << QByteArray("/0/0")
            << QByteArray("plaintext");

    QTest::newRow("torture-richtext")
            << bsTortureTest
            << QByteArray("2.2.1")
            << (pathList() << qMakePair(0,0) << qMakePair(1,0)
                << qMakePair(0,0) << qMakePair(1,0) << qMakePair(0,0))
            << QByteArray("/0/1/0/1/0")
            << QByteArray("text richtext");
}

void CryptographyMessageModelTest::testCustomMessageParts()
{
    //initialize model with a root item
    QStandardItemModel *minimal2 = new QStandardItemModel();
    QStandardItem *dummy_root = new QStandardItem();
    QStandardItem *root_mime = new QStandardItem("multipart/mixed");
    dummy_root->appendRow(root_mime);
    minimal2->invisibleRootItem()->appendRow(dummy_root);

    // Make shure we didn't mess up until here
    QVERIFY(minimal2->index(0,0).child(0,0).isValid());
    QCOMPARE(minimal2->index(0,0).child(0,0).data(), root_mime->data(Qt::DisplayRole));

    Cryptography::MessageModel msgModel(0, minimal2->index(0,0));

    QModelIndex rootPartIndex = msgModel.index(0,0).child(0,0);

    QCOMPARE(rootPartIndex.data(), root_mime->data(Qt::DisplayRole));

    Cryptography::LocalMessagePart *localRoot = new Cryptography::LocalMessagePart(nullptr, "multipart/mixed");
    Cryptography::LocalMessagePart *localText = new Cryptography::LocalMessagePart(nullptr, "text/plain");
    localText->setData("foobar");
    localRoot->setChild(0, 0, localText);
    Cryptography::LocalMessagePart *localHtml = new Cryptography::LocalMessagePart(nullptr, "text/html");
    localHtml->setData("<html>foobar</html>");
    localRoot->setChild(1, 0, localHtml);

    QVector<Cryptography::MessagePart*> v;
    v.insert(0, localRoot);
    msgModel.insertSubtree(rootPartIndex, v);

    QVERIFY(msgModel.rowCount(rootPartIndex) > 0);

    QModelIndex localRootIndex = rootPartIndex.child(0, 0);
    QVERIFY(localRootIndex.isValid());
    QCOMPARE(localRootIndex.data(Imap::Mailbox::RolePartMimeType), localRoot->data(Imap::Mailbox::RolePartMimeType));
    QModelIndex localTextIndex = localRootIndex.child(0, 0);
    QVERIFY(localTextIndex.isValid());
    QCOMPARE(localTextIndex.data(Imap::Mailbox::RolePartMimeType), localText->data(Imap::Mailbox::RolePartMimeType));
    QModelIndex localHtmlIndex = localRootIndex.child(1, 0);
    QVERIFY(localHtmlIndex.isValid());
    QCOMPARE(localHtmlIndex.data(Imap::Mailbox::RolePartMimeType), localHtml->data(Imap::Mailbox::RolePartMimeType));
}

void CryptographyMessageModelTest::testMixedMessageParts()
{

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
    cServer("* 1 FETCH (UID 333 BODYSTRUCTURE (" + bsPlaintext + "))\r\n" + t.last("OK fetched\r\n"));
    cEmpty();
    QVERIFY(model->rowCount(msg) > 0);
    Cryptography::MessageModel msgModel(0, msg);
    QModelIndex mappedMsg = msgModel.index(0,0);
    QVERIFY(mappedMsg.isValid());
    QVERIFY(msgModel.rowCount(mappedMsg) > 0);

    QModelIndex mappedPart = mappedMsg.child(0, 0);
    QVERIFY(mappedPart.isValid());

    QCOMPARE(mappedPart.data(Imap::Mailbox::RolePartData).toString(), QString());
    QByteArray data = "blesmrt";
    cClient(t.mk("UID FETCH 333 (BODY.PEEK[1])\r\n"));
    cServer("* 1 FETCH (UID 333 BODY[1] {" + QByteArray::number(data.size()) + "}\r\n" +
            data + ")\r\n" + t.last("OK fetched\r\n"));

    QCOMPARE(mappedPart.data(Imap::Mailbox::RolePartData).toByteArray(), data);
    QCOMPARE(mappedPart.data(Imap::Mailbox::RolePartPathToPart).toByteArray(), QByteArray("/0"));

    cEmpty();
    QVERIFY(errorSpy->isEmpty());

    Cryptography::LocalMessagePart *localRoot = new Cryptography::LocalMessagePart(nullptr, "multipart/mixed");
    Cryptography::LocalMessagePart *localText = new Cryptography::LocalMessagePart(nullptr, "text/plain");
    localText->setData("foobar");
    localRoot->setChild(0, 0, localText);
    Cryptography::LocalMessagePart *localHtml = new Cryptography::LocalMessagePart(nullptr, "text/html");
    localHtml->setData("<html>foobar</html>");
    localRoot->setChild(1, 0, localHtml);

    QVector<Cryptography::MessagePart*> v;
    v.insert(0, localRoot);
    msgModel.insertSubtree(mappedPart, v);

    QVERIFY(msgModel.rowCount(mappedPart) > 0);

    QModelIndex localRootIndex = mappedPart.child(0, 0);
    QVERIFY(localRootIndex.isValid());
    QCOMPARE(localRootIndex.data(Imap::Mailbox::RolePartMimeType), localRoot->data(Imap::Mailbox::RolePartMimeType));
    QModelIndex localTextIndex = localRootIndex.child(0, 0);
    QVERIFY(localTextIndex.isValid());
    QCOMPARE(localTextIndex.data(Imap::Mailbox::RolePartMimeType), localText->data(Imap::Mailbox::RolePartMimeType));
    QModelIndex localHtmlIndex = localRootIndex.child(1, 0);
    QVERIFY(localHtmlIndex.isValid());
    QCOMPARE(localHtmlIndex.data(Imap::Mailbox::RolePartMimeType), localHtml->data(Imap::Mailbox::RolePartMimeType));
}

TROJITA_HEADLESS_TEST( CryptographyMessageModelTest )
