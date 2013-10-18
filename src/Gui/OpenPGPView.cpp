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
#include "OpenPGPView.h"

#include <QLabel>
#include <QVBoxLayout>

#include <QDebug>

#include "MessageView.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"

namespace Gui
{

OpenPGPView::OpenPGPView(QWidget *parent, const QModelIndex &partIndex)
    : QGroupBox(parent), m_model(0), m_partIndex(partIndex)
{
    setFlat(true);
    using namespace Imap::Mailbox;
}

void OpenPGPView::startVerification(PartWidgetFactory* factory, const int recursionDepth, const PartWidgetFactory::PartLoadingOptions options)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    uint childrenCount = m_partIndex.model()->rowCount(m_partIndex);
    if (childrenCount == 1) {
        setTitle(tr("Malformed multipart/signed message: only one nested part"));
        QModelIndex anotherPart = m_partIndex.child(0, 0);
        Q_ASSERT(anotherPart.isValid()); // guaranteed by the MVC
        layout->addWidget(factory->create(anotherPart, recursionDepth + 1, options));
    } else if (childrenCount != 2) {
        QLabel *lbl = new QLabel(tr("Malformed multipart/signed message: %1 nested parts").arg(QString::number(childrenCount)), this);
        layout->addWidget(lbl); //TODO: show those parts here?
        return;
    } else {
        setTitle(tr("OpenPGP signed message"));
        Q_ASSERT(childrenCount == 2); // from the if logic; FIXME: refactor

        m_pgp = new QCA::OpenPGP();
        m_msg = new QCA::SecureMessage(m_pgp);
        Imap::Mailbox::Model::realTreeItem(m_partIndex, &m_model);
        Q_ASSERT(m_model);
        connect(m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChangedForVerification(QModelIndex,QModelIndex)));

        QModelIndex anotherPart = m_partIndex.child(0, 0);
        Q_ASSERT(anotherPart.isValid()); // guaranteed by the MVC
        layout->addWidget(factory->create(anotherPart, recursionDepth + 1, options));

        //Trigger lazy loading of the required message parts
        m_partIndex.child(0,0).data(Imap::Mailbox::RolePartData);
        m_partIndex.child(0,0).child(0,Imap::Mailbox::TreeItem::OFFSET_MIME).data(Imap::Mailbox::RolePartData);
        m_partIndex.child(1,0).data(Imap::Mailbox::RolePartData);

        //call handleDataChanged at least once in case all parts are already available
        handleDataChangedForVerification(QModelIndex(),QModelIndex());
    }
}

void OpenPGPView::startDecryption(PartWidgetFactory* factory, const int recursionDepth, const PartWidgetFactory::PartLoadingOptions options)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    uint childrenCount = m_partIndex.model()->rowCount(m_partIndex);
    if (childrenCount == 1) {
        setTitle(tr("Malformed multipart/encrypted message: only one nested part"));
        QModelIndex anotherPart = m_partIndex.child(0, 0);
        Q_ASSERT(anotherPart.isValid()); // guaranteed by the MVC
        layout->addWidget(factory->create(anotherPart, recursionDepth + 1, options));
    } else if (childrenCount != 2) {
        QLabel *lbl = new QLabel(tr("Malformed multipart/encrypted message: %1 nested parts").arg(QString::number(childrenCount)), this);
        layout->addWidget(lbl); //TODO: show those parts here?
        return;
    } else {
        setTitle(tr("OpenPGP encrypted message"));
        Q_ASSERT(childrenCount == 2); // from the if logic; FIXME: refactor

        m_pgp = new QCA::OpenPGP();
        m_msg = new QCA::SecureMessage(m_pgp);
        Imap::Mailbox::Model::realTreeItem(m_partIndex, &m_model);
        Q_ASSERT(m_model);
        connect(m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChangedForDecryption(QModelIndex,QModelIndex)));

        //Trigger lazy loading of the required message parts
        m_partIndex.child(0,0).data(Imap::Mailbox::RolePartData);
        m_partIndex.child(1,0).data(Imap::Mailbox::RolePartData);

        //call handleDataChanged at least once in case all parts are already available
        handleDataChangedForDecryption(QModelIndex(),QModelIndex());
    }
}

QString OpenPGPView::quoteMe() const
{
    QStringList res;
    for (QObjectList::const_iterator it = children().begin(); it != children().end(); ++it) {
        const AbstractPartWidget *w = dynamic_cast<const AbstractPartWidget *>(*it);
        if (w)
            res += w->quoteMe();
    }
    return res.join("\n");
}

void OpenPGPView::handleDataChangedForVerification(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft)
    Q_UNUSED(bottomRight)

    qDebug() << "handleDataChanged";
    if ( m_partIndex.child(0,0).data(Imap::Mailbox::RoleIsFetched).toBool() &&
         m_partIndex.child(0,0).child(0,Imap::Mailbox::TreeItem::OFFSET_MIME).data(Imap::Mailbox::RoleIsFetched).toBool() &&
         m_partIndex.child(1,0).data(Imap::Mailbox::RoleIsFetched).toBool() )
    {
        qDebug() << "data fetched";
        disconnect(m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChangedForVerification(QModelIndex,QModelIndex)));

        verify(m_partIndex.child(0,0), m_partIndex.child(1,0));
    }

}

void OpenPGPView::handleDataChangedForDecryption(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft)
    Q_UNUSED(bottomRight)

    qDebug() << "handleDataChanged";
    if ( m_partIndex.child(0,0).data(Imap::Mailbox::RoleIsFetched).toBool() &&
         m_partIndex.child(1,0).data(Imap::Mailbox::RoleIsFetched).toBool() )
    {
        qDebug() << "data fetched";
        disconnect(m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChangedForDecryption(QModelIndex,QModelIndex)));

        decrypt(m_partIndex.child(0,0), m_partIndex.child(1,0));
    }

}

void OpenPGPView::reloadContents()
{
    Q_FOREACH( QObject* const obj, children() ) {
        /*qDebug() << obj->metaObject()->className();*/
        AbstractPartWidget* w = dynamic_cast<AbstractPartWidget*>( obj );
        if ( w ) {
            /*qDebug() << "reloadContents:" << w;*/
            w->reloadContents();
        }
    }
}

QString OpenPGPView::strerror(QCA::SecureMessage::Error error)
{
    switch(error)
    {
    case QCA::SecureMessage::ErrorCertKeyMismatch:
        return tr("certificate and private key don't match");
    case QCA::SecureMessage::ErrorPassphrase:
        return tr("passphrase was either wrong or not provided");
    case QCA::SecureMessage::ErrorFormat:
        return tr("input format was bad");
    case QCA::SecureMessage::ErrorSignerExpired:
        return tr("signing key is expired");
    case QCA::SecureMessage::ErrorSignerInvalid:
        return tr("signing key is invalid in some way");
    case QCA::SecureMessage::ErrorEncryptExpired:
        return tr("encrypting key is expired");
    case QCA::SecureMessage::ErrorEncryptUntrusted:
        return tr("encrypting key is untrusted");
    case QCA::SecureMessage::ErrorEncryptInvalid:
        return tr("encrypting key is invalid in some way");
    case QCA::SecureMessage::ErrorNeedCard:
        return tr("pgp card is missing");
    default:
        return tr("other error");
    }
}

void OpenPGPView::verify(const QModelIndex &textIndex, const QModelIndex &sigIndex)
{
    QModelIndex mimePart = textIndex.child(0, Imap::Mailbox::TreeItem::OFFSET_MIME);
    Q_ASSERT(mimePart.isValid());

    QByteArray rawsigned_text = mimePart.data(Imap::Mailbox::RolePartData).toByteArray();
    rawsigned_text += textIndex.data(Imap::Mailbox::RolePartRawData).toByteArray();

    qDebug() << rawsigned_text.data();
    qDebug() << sigIndex.data(Imap::Mailbox::RolePartData).toByteArray();

    connect(m_msg, SIGNAL(finished()), this, SLOT(slotDisplayVerificationResult()));

    m_msg->setFormat(QCA::SecureMessage::Ascii);
    m_msg->startVerify(sigIndex.data(Imap::Mailbox::RolePartData).toByteArray());
    m_msg->update(rawsigned_text.data());
    m_msg->end();
}

void OpenPGPView::slotDisplayVerificationResult()
{
    QLabel *lbl = new QLabel((QWidget*) parent());
    QFrame *pgpFrame = new QFrame();
    pgpFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    //TODO: set background color depending on result

    if (!m_msg->success())
    {
        lbl->setText(tr("Signature verification failed. %1").arg(strerror(m_msg->errorCode())));
        qDebug() << "failed: " << m_msg->diagnosticText();
    } else {
        QString signer;
        QCA::KeyStoreManager manager;
        QCA::KeyStoreManager::start();
        manager.waitForBusyFinished();
        QCA::KeyStore store("qca-gnupg", &manager);
        QCA::PGPKey key = m_msg->signer().key().pgpPublicKey();
        Q_FOREACH(QCA::KeyStoreEntry entry, store.entryList())
        {
            if ( entry.id() == key.keyId() )
            {
                if ( !key.isNull() )
                {
                    //signer = key.primaryUserId();
                }
            }
        }
        lbl->setText(tr("Signature with key \"%1\" (%2) successfully verified.").arg(signer, key.keyId()));
    }
    QVBoxLayout *l = new QVBoxLayout();
    l->addWidget(lbl);
    pgpFrame->setLayout(l);
    this->layout()->addWidget(pgpFrame);
}

void OpenPGPView::decrypt(const QModelIndex &versionIndex, const QModelIndex &encIndex)
{
    if ( !versionIndex.data(Imap::Mailbox::RolePartData).toString().contains(QLatin1String("Version: 1")) ) //TODO: replace != with some regex matching or similar
    {
        this->layout()->addWidget(new QLabel(tr("Unsupported Version"))); //TODO: this message should be more helpful
        return;
    }

    connect(m_msg, SIGNAL(finished()), this, SLOT(slotDisplayDecryptionResult()));

    m_msg->setFormat(QCA::SecureMessage::Ascii);
    m_msg->startDecrypt();
    m_msg->update(encIndex.data(Imap::Mailbox::RolePartData).toByteArray().data());
    m_msg->end();
}

void OpenPGPView::slotDisplayDecryptionResult()
{
    QLabel *lbl = new QLabel((QWidget*) parent());

    if (!m_msg->success())
    {
        lbl->setText(tr("Decryption failed. %1").arg(strerror(m_msg->errorCode())));
        qDebug() << "failed: " << m_msg->diagnosticText();
    } else {
        lbl->setText(tr("Message decrypted")); //TODO: what to do with the decrypted message
        qDebug() << "Message:";
        while ( m_msg->bytesAvailable() )
            qDebug() << m_msg->read();
    }
    QVBoxLayout *l = new QVBoxLayout();
    l->addWidget(lbl);
    QFrame *pgpFrame = new QFrame();
    pgpFrame->setLayout(l);
    pgpFrame->setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    this->layout()->addWidget(pgpFrame);
}

}
