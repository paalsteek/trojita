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

#ifndef CRYPTOGRAPHY_MESSAGEPARTFACTORY_H
#define CRYPTOGRAPHY_MESSAGEPARTFACTORY_H

#include <QObject>
#include <QSettings>

class QModelIndex;

namespace Cryptography {
class OpenPGPHelper;
class SMIMEHelper;
}
namespace Cryptography {
class MessageModel;
class MessagePart;

/** @short Factory to populate a MessageModel with all remote message parts,
 * decrypt encrypted messages and add the decrypted content to the MessageModel
 */
class MessagePartFactory: public QObject {
    Q_OBJECT
public:
    MessagePartFactory(MessageModel* model, QSettings *settings);

public slots:
    /** @short Build a subtree of either ProxyMessageParts for an existing message
     * or of LocalMessageParts after decrypting an encrypted message and add that
     * tree to our MessageModel
     */
    void buildSubtree(const QModelIndex& parent);

private:
    /** @short Build the tree of ProxyMessageParts for a given message */
    void buildProxyTree(const QModelIndex& source, MessagePart* destination);

    MessageModel *m_model;
    Cryptography::OpenPGPHelper *m_pgpHelper;
    Cryptography::SMIMEHelper *m_cmsHelper;
};
}
#endif /* CRYPTOGRAPHY_MESSAGEPARTFACTORY_H */
