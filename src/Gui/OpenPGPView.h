/* Copyright (C) 2013 Stephan Platz <trojita@paalsteek.de>

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
#ifndef OPENPGPVIEW_H
#define OPENPGPVIEW_H

#include <QtCrypto/QtCrypto>

#include <QGroupBox>
#include <QModelIndex>
#include <QVBoxLayout>
#include "AbstractPartWidget.h"
#include "PartWidgetFactory.h"

namespace Imap
{
namespace Network
{
class MsgPartNetAccessManager;
}
}

namespace Gui
{

class MessageView;

/** @short TODO
 */
class OpenPGPView : public QGroupBox, public AbstractPartWidget
{
    Q_OBJECT

public:
    OpenPGPView(QWidget *parent, PartWidgetFactory *factory, const QModelIndex &partIndex, const int recursionDepth, const PartWidgetFactory::PartLoadingOptions options);
    virtual QString quoteMe() const;
    virtual void reloadContents();
    void startVerification();
    void startDecryption();

private slots:
    void handleDataChangedForVerification(const QModelIndex &topLeft, const QModelIndex &bottomDown);
    void handleDataChangedForDecryption(const QModelIndex &topLeft, const QModelIndex &bottomDown);
    void slotDisplayVerificationResult();
    void slotDisplayDecryptionResult();

private:
    void verify(const QModelIndex &textIndex, const QModelIndex &sigIndex);
    void decrypt(const QModelIndex &versionIndex, const QModelIndex &encIndex);
    QString strerror(QCA::SecureMessage::Error error);
    const Imap::Mailbox::Model *m_model;
    QPersistentModelIndex m_partIndex;
    PartWidgetFactory *m_factory;
    const int m_recursionDepth;
    PartWidgetFactory::PartLoadingOptions m_options;
    QCA::SecureMessage *m_msg;
    QCA::OpenPGP *m_pgp;
};
}

#endif // OPENPGPVIEW_H
