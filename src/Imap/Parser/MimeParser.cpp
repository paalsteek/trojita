#include "MimeParser.h"

#include <string>
#include "mimetic/mimetic.h"

#include "../Encoders.h"

namespace Imap {

MimeParser::MimeParser()
{
}

void MimeParser::parseMessage(QByteArray message, Mailbox::TreeItemPart *parent)
{
    mimetic::MimeEntity me(message.begin(), message.end());
    QList<Mailbox::TreeItem *> items;
    items.append(createTreeItems(&me, parent));
    parent->setChildren(items); //TODO: delete any existing children
}

void MimeParser::storeInterestingFields(Mailbox::TreeItemPart *part, mimetic::Header *header)
{
    mimetic::ContentType type = header->contentType();
    QString charset = QString::fromStdString(type.param("charset"));
    if ( ! charset.isEmpty() )
        part->setCharset(charset);

    QString protocol = QString::fromStdString(type.param("protocol"));
    if ( ! protocol.isEmpty() )
        part->setProtocol(protocol);

    QString fileName = QString::fromStdString(type.param("name"));
    if ( ! fileName.isEmpty() )
        part->setFileName(fileName);

    part->setEncoding(QString::fromStdString(header->contentTransferEncoding().str()).toLatin1());
    part->setBodyDisposition(QString::fromStdString(header->contentDisposition().str()).toLatin1());
}

Mailbox::TreeItem *MimeParser::createTreeItems(mimetic::MimeEntity *pMe, Mailbox::TreeItemPart *parent)
{
    mimetic::Header &h = pMe->header();
    qDebug() << "new message part found:" << QString::fromStdString(h.contentType().str());
    Mailbox::TreeItemPart *part = new Mailbox::TreeItemPart(parent, QString::fromStdString(h.contentType().type() + "/" + h.contentType().subtype()));
    storeInterestingFields(part, &h);
    if ( h.contentType().isMultipart() )
    {
        mimetic::MimeEntityList& parts = pMe->body().parts();
        QList<Mailbox::TreeItem *> items;
        for (mimetic::MimeEntityList::iterator mbit = parts.begin(); mbit != parts.end(); ++mbit)
        {
            items.append(createTreeItems(*mbit, part));
        }
        part->setChildren(items);
    } else {
        std::stringstream s;
        s << pMe->body();
        part->m_rawData = QString::fromStdString(s.str()).toLatin1();
        decodeTransportEncoding(QString::fromStdString(s.str()).toLatin1(), part->encoding(), part->dataPtr());
    }
    part->m_fetchStatus = Mailbox::TreeItem::DONE;
    return part;
}

}
