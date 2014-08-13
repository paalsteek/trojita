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

#ifndef CRYPTOGRAPHY_MESSAGEMODEL_H
#define CRYPTOGRAPHY_MESSAGEMODEL_H

#include <QHash>
#include <QObject>
#include <QPersistentModelIndex>

#include <QDebug>

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

    MessagePart* parent() const;
    const int row() const;
    MessagePart* child(int row, int column) const;
    int rowCount() const;

    virtual QVariant data(int role) const = 0;

    void setChild(int row, int column, MessagePart* part);
    void setRow(int row);
    void setParent(MessagePart *parent);

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

    QVariant data(int role) const;

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

    void setData(const QByteArray& data);
    void setFetchingState(FetchingState state);
    FetchingState getFetchingState() const;

    void setCharset(const QString& charset);
    void setContentFormat(const QString& format);
    void setDelSp(const QString& delSp);
    void setFilename(const QString& filename);
    void setEncoding(const QByteArray& encoding);
    void setBodyFldId(const QByteArray& bodyFldId);
    void setBodyDisposition(const QByteArray& bodyDisposition);
    void setMultipartRelatedStartPart(const QByteArray& startPart);
    void setOctets(int octets);
    void setEnvelope(Imap::Message::Envelope *envelope);

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

/** @short A model for a single message including raw and possibly decrypted message parts */
class MessageModel: public QAbstractItemModel
{
    Q_OBJECT

public:
    MessageModel(QObject *parent, const QModelIndex& message);
    ~MessageModel();

    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex &child) const;
    int rowCount(const QModelIndex &parent) const;
    int columnCount(const QModelIndex &parent) const;
    QVariant data(const QModelIndex &index, int role) const;

    QModelIndex message() const;

    /** @short Adds a mapping from a source QModelIndex to a MessagePart
     * to support handling of signals from the original model
     */
    void addIndexMapping(QModelIndex source, MessagePart* destination);

public slots:
    void insertSubtree(const QModelIndex& parent, const QVector<Cryptography::MessagePart*>& children);
    void mapDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);

signals:
    void passwordRequired(int id, const QString& subject);
    void passwordAvailable(int id, const QString& password);
    void passwordError(int id);

protected:
    const QPersistentModelIndex m_message;
    QHash<QModelIndex, MessagePart*> m_map;
    MessagePart *m_rootPart;
    MessagePartFactory *m_factory;
};
}
#endif /* CRYPTOGRAPHY_MESSAGEMODEL_H */
