#include "MimeParser.h"

#include <string>
#include "mimetic/mimetic.h"

#include "../Encoders.h"
#include "../Model/MailboxTreeFwd.h"

namespace Imap {

MimeParser::MimeParser()
{
}

void MimeParser::parseMessage(QByteArray message, Mailbox::TreeItemPart *parent)
{
    mimetic::MimeEntity me(message.begin(), message.end());
    Mailbox::TreeItemChildrenList items;
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
    part->setFetchStatus(Mailbox::TreeItem::LOADING);
    storeInterestingFields(part, &h);
    if ( h.contentType().isMultipart() )
    {
        mimetic::MimeEntityList& parts = pMe->body().parts();
        Mailbox::TreeItemChildrenList items;
        for (mimetic::MimeEntityList::iterator mbit = parts.begin(); mbit != parts.end(); ++mbit)
        {
            items.append(createTreeItems(*mbit, part));
        }
        part->setChildren(items);
    } else {
        std::stringstream s;
        s << pMe->body();
        QByteArray rawData = QString::fromStdString(s.str()).toLatin1();
        Mailbox::TreeItemPart* rawPart = dynamic_cast<Mailbox::TreeItemPart*>(part->specialColumnPtr(0, Mailbox::TreeItem::OFFSET_RAW_CONTENTS));
        rawPart->m_data = rawData;
        rawPart->setFetchStatus(Mailbox::TreeItemPart::DONE);
        decodeContentTransferEncoding(rawData, part->encoding(), part->dataPtr());
        qDebug() << "part decoded:" << *(part->dataPtr());
    }
    part->setFetchStatus(Mailbox::TreeItem::DONE);
    return part;
}

}
