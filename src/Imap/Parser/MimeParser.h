#ifndef MIMEPARSER_H
#define MIMEPARSER_H

#include "../Model/MailboxTree.h"

namespace mimetic {

class MimeEntity;
class Header;

}

namespace Imap {

class MimeParser
{
public:
    MimeParser();
    void parseMessage(QByteArray message, Mailbox::TreeItemPart *parent);

private:
    Mailbox::TreeItem *createTreeItems(mimetic::MimeEntity *pMe, Mailbox::TreeItemPart *parent);
    void storeInterestingFields(Mailbox::TreeItemPart *part, mimetic::Header *header);
};

}

#endif // MIMEPARSER_H
