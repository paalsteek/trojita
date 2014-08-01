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
#include <QObject>
#include <QPersistentModelIndex>

#include <QDebug>

#include "configure.cmake.h"

#ifdef TROJITA_HAVE_MIMETIC
#include <mimetic/mimetic.h>
#endif /* TROJITA_HAVE_MIMETIC */

namespace Cryptography {
class OpenPGPHelper;
class SMIMEHelper;
}

namespace Imap {
namespace Message {
class Envelope;
}
}

namespace Cryptography {
class MessagePartFactory;
class MessageModel;

class MessagePart
{
public:
    MessagePart(MessagePart *parent);
    virtual ~MessagePart();

    MessagePart* parent() const { return m_parent; }
    const int row() const { return m_row; }
    MessagePart* child(int row, int column) const;
    int rowCount() const { return m_children.size(); }

    virtual QVariant data(int role) const = 0;

    void setChild(int row, int column, MessagePart* part);
    void setRow(int row) { m_row = row; }
    void setParent(MessagePart *parent) { m_parent = parent; }

    void setRawPart(MessagePart *rawPart);

private:
    MessagePart *m_parent;
    QVector<MessagePart*> m_children;
    MessagePart *m_rawPart;
    int m_row;
};

class ProxyMessagePart : public MessagePart
{
public:
    ProxyMessagePart(MessagePart *parent, const QModelIndex &sourceIndex, Cryptography::MessageModel *model);
    ~ProxyMessagePart();

    QVariant data(int role) const { return m_sourceIndex.data(role); }

private:
    QPersistentModelIndex m_sourceIndex;
};

class LocalMessagePart : public MessagePart
{
public:
    /** @short Availability of an item */
    enum FetchingState {
        NONE, /**< @short No attempt to decrypt/load an item has been made yet */
        UNAVAILABLE, /**< @short Item could not be decrypted/loaded */
        LOADING, /**< @short Decryption/Loading of an item is already scheduled */
        DONE /**< @short Item is available right now */
    };
    LocalMessagePart(MessagePart *parent, const QByteArray &mimetype);
    ~LocalMessagePart();

    QVariant data(int role) const;

    void setData(const QByteArray& data) { m_data = data; m_state = DONE; }
    void setFetchingState(FetchingState state) { m_state = state; }
    FetchingState getFetchingState() const { return m_state; }

    void setCharset(const QString& charset) { m_charset = charset; }
    void setContentFormat(const QString& format) { m_contentFormat = format; }
    void setDelSp(const QString& delSp) { m_delSp = delSp; }
    void setFilename(const QString& filename) { m_filename = filename; }
    void setEncoding(const QByteArray& encoding) { m_encoding = encoding; }
    void setBodyFldId(const QByteArray& bodyFldId) { m_bodyFldId = bodyFldId; }
    void setBodyDisposition(const QByteArray& bodyDisposition) { m_bodyDisposition = bodyDisposition; }
    void setMultipartRelatedStartPart(const QByteArray& startPart) { m_multipartRelatedStartPart = startPart; }
    void setOctets(int octets) { m_octets = octets; }
    void setEnvelope(Imap::Message::Envelope *envelope) { m_envelope = envelope; }

private:
    bool isTopLevelMultipart() const;
    QString partId() const;
    QString pathToPart() const;

protected:
    Imap::Message::Envelope *m_envelope;
    FetchingState m_state;
    QString m_charset;
    QString m_contentFormat;
    QString m_delSp;
    QString m_filename;
    QByteArray m_data;
    QByteArray m_mimetype;
    QByteArray m_encoding;
    QByteArray m_bodyFldId;
    QByteArray m_bodyDisposition;
    QByteArray m_multipartRelatedStartPart;
    uint m_octets;
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

    QModelIndex message() const { return m_message; }

    void addIndexMapping(QModelIndex source, MessagePart* destination) { m_map.insert(source, destination); }

public slots:
    void insertSubtree(const QModelIndex& parent, const QVector<Cryptography::MessagePart*>& children);
    void mapDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);

protected:
    const QPersistentModelIndex m_message;
    QHash<QModelIndex, MessagePart*> m_map;
    MessagePart *m_rootPart;
    MessagePartFactory *m_factory;
};
}
#endif /* COMMON_MODEL_H */
