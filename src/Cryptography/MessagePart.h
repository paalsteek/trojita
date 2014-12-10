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

#ifndef CRYPTOGRAPHY_MESSAGEPART_H
#define CRYPTOGRAPHY_MESSAGEPART_H

#include <memory>
#include <QModelIndex>
#include <QVector>
#include <QUrl>
#include <QVector>

namespace Imap {
namespace Message {
class Envelope;
}
}

namespace Cryptography {
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
    std::unique_ptr<MessagePart> m_rawPart;
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
    FetchingState fetchingState() const;

    void setCharset(const QByteArray& charset);
    void setContentFormat(const QByteArray &format);
    void setDelSp(const QByteArray& delSp);
    void setFilename(const QString& filename);
    void setEncoding(const QByteArray& encoding);
    void setBodyFldId(const QByteArray& bodyFldId);
    void setBodyDisposition(const QByteArray& bodyDisposition);
    void setMultipartRelatedStartPart(const QByteArray& startPart);
    void setOctets(uint octets);
    void setEnvelope(Imap::Message::Envelope *envelope);
    void setHdrReferences(const QList<QByteArray>& references);
    void setHdrListPost(const QList<QUrl>& listPost);
    void setHdrListPostNo(const bool listPostNo);

private:
    bool isTopLevelMultipart() const;
    QByteArray partId() const;
    QByteArray pathToPart() const;

protected:
    Imap::Message::Envelope *m_envelope;
    QList<QByteArray> m_hdrReferences;
    QList<QUrl> m_hdrListPost;
    bool m_hdrListPostNo;
    FetchingState m_state;
    QByteArray m_charset;
    QByteArray m_contentFormat;
    QByteArray m_delSp;
    QByteArray m_data;
    QByteArray m_mimetype;
    QByteArray m_encoding;
    QByteArray m_bodyFldId;
    QByteArray m_bodyDisposition;
    QByteArray m_multipartRelatedStartPart;
    QString m_filename;
    uint m_octets;
};

}

#endif /* CRYPTOGRAPHY_MESSAGEPART_H */
