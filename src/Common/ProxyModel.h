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

#include "Cryptography/OpenPGPHelper.h"
#include "Cryptography/SMIMEHelper.h"

namespace Common {

class MessagePart : public QObject {
    Q_OBJECT

public:
    MessagePart(MessagePart *parent, int row);
    virtual ~MessagePart();

    MessagePart* parent() { return m_parent; }
    const int row() { return m_row; }
    virtual MessagePart* child(int row);
    virtual int rowCount() const = 0;

    virtual QVariant data(int role) = 0;

    void replaceChild(int row, MessagePart* part);

signals:
    void partChanged();

private:
    virtual MessagePart* newChild(int row) = 0;

    MessagePart *m_parent;
    QHash<int, MessagePart*> children;
    int m_row;
};

class ProxyMessagePart : public MessagePart {
    Q_OBJECT

public:
    ProxyMessagePart(MessagePart *parent, int row, const QModelIndex &sourceIndex);
    ~ProxyMessagePart();

    MessagePart* newChild(int row);
    int rowCount() const;

    QVariant data(int role) { return m_sourceIndex.data(role); }

public slots:
    void handleSourceDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight);

private:
    QPersistentModelIndex m_sourceIndex;
};

class LocalMessagePart : public MessagePart {
public:
    LocalMessagePart(MessagePart *parent, int row, mimetic::MimeEntity* pMe);
    ~LocalMessagePart();

    MessagePart* newChild(int row);
    int rowCount() const;

    QVariant data(int role);

private:
    QString mimetype() const;
    int octets() const;
    QString charset() const;
    QString protocol() const;
    QString filename() const;
    QString format() const;
    QString delsp() const;
    QByteArray data() const;
    QByteArray transferEncoding() const;
    QByteArray bodyDisposition() const;
    QByteArray bodyFldId() const;
    QByteArray relatedMainCid() const;

    QString partId() const;
    QString pathToPart() const;

    bool isTopLevelMultipart() const;

protected:
    mimetic::MimeEntity *m_me;
};

class EncryptedMessagePart : public LocalMessagePart {
    Q_OBJECT

public:
    EncryptedMessagePart(MessagePart *parent, int row, MessagePart *raw);

    QVariant data(int role) { return LocalMessagePart::data(role); }

    MessagePart* rawPart() { return m_raw; }

signals:
    void partDecrypted();

public slots:
    void handleDataDecrypted(mimetic::MimeEntity* pMe);

protected:
    MessagePart* m_raw;
};

class MessageModel: public QAbstractItemModel
{
    Q_OBJECT

public:
    MessageModel(QObject *parent, const QModelIndex& message);
    ~MessageModel();

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const { return !m_rootPart ? 0 : 1; }
    QVariant data(const QModelIndex &index, int role) const;

    QModelIndex message() { return m_message; }

private slots:
    void handlePartChanged();
    void handlePartDecrypted();

protected:
    const QPersistentModelIndex m_message;
    mutable MessagePart *m_rootPart;
    Cryptography::OpenPGPHelper* m_pgpHelper;
    Cryptography::SMIMEHelper* m_smimeHelper;
};
}
#endif /* COMMON_MODEL_H */
