#include "OpenPGPView.h"

#include <QLabel>
#include <QVBoxLayout>

#include <QtCrypto/QtCrypto>

#include <QDebug>

#include "MessageView.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"
#include "Imap/Model/Model.h"

namespace {

    /** @short Unset flags which only make sense for one level of nesting */
    Gui::PartWidgetFactory::PartLoadingOptions filteredForEmbedding(const Gui::PartWidgetFactory::PartLoadingOptions options)
    {
        return options & Gui::PartWidgetFactory::MASK_PROPAGATE_WHEN_EMBEDDING;
    }
}

namespace Gui
{

OpenPGPView::OpenPGPView(QWidget *parent, PartWidgetFactory *factory,
                         const QModelIndex &partIndex, const int recursionDepth,
                         const PartWidgetFactory::PartLoadingOptions options)
    : QGroupBox(tr("OpenPGP signed message"), parent), m_model(0), m_partIndex(partIndex)
{
    setFlat(true);
    using namespace Imap::Mailbox;
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    uint childrenCount = partIndex.model()->rowCount(partIndex);
    if (childrenCount == 1) {
        setTitle(tr("Malformed multipart/signed message: only one nested part"));
        QModelIndex anotherPart = partIndex.child(0, 0);
        Q_ASSERT(anotherPart.isValid()); // guaranteed by the MVC
        layout->addWidget(factory->create(anotherPart, recursionDepth + 1, filteredForEmbedding(options)));
    } else if (childrenCount != 2) {
        QLabel *lbl = new QLabel(tr("Malformed multipart/signed message: %1 nested parts").arg(QString::number(childrenCount)), this);
        layout->addWidget(lbl);
        return;
    } else {
        Q_ASSERT(childrenCount == 2); // from the if logic; FIXME: refactor
        QModelIndex anotherPart = partIndex.child(0, 0);
        Q_ASSERT(anotherPart.isValid()); // guaranteed by the MVC
        layout->addWidget(factory->create(anotherPart, recursionDepth + 1, filteredForEmbedding(options)));

        Imap::Mailbox::Model::realTreeItem(partIndex, &m_model);
        Q_ASSERT(m_model);
        connect(m_model, SIGNAL(dataChanged(QModelIndex, QModelIndex)), this, SLOT(handleDataChanged(QModelIndex,QModelIndex)));

        Imap::Mailbox::TreeItemPart *partPart = dynamic_cast<Imap::Mailbox::TreeItemPart *>(Imap::Mailbox::Model::realTreeItem(partIndex, &m_model));

        partPart->child(0,0)->fetch(const_cast<Imap::Mailbox::Model *>(m_model));
        partPart->child(0,0)->specialColumnPtr(0,Imap::Mailbox::TreeItem::OFFSET_MIME)->fetch(const_cast<Imap::Mailbox::Model *>(m_model));
        partPart->child(1,0)->fetch(const_cast<Imap::Mailbox::Model *>(m_model));
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

void OpenPGPView::handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomRight)
{
    Q_UNUSED(topLeft)
    Q_UNUSED(bottomRight)

    qDebug() << "handleDataChanged";
    if ( m_partIndex.child(0,0).data(Imap::Mailbox::RoleIsFetched).toBool() &&
         m_partIndex.child(0,0).child(0,Imap::Mailbox::TreeItem::OFFSET_MIME).data(Imap::Mailbox::RoleIsFetched).toBool() &&
         m_partIndex.child(1,0).data(Imap::Mailbox::RoleIsFetched).toBool() )
    {
        qDebug() << "data fetched";
        disconnect(m_model, SIGNAL(dataChanged(QModelIndex,QModelIndex)), this, SLOT(handleDataChanged(QModelIndex,QModelIndex)));

        verify(m_partIndex.child(0,0), m_partIndex.child(1,0));
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

QString strerror(QCA::SecureMessage::Error error)
{
    switch(error)
    {
    case QCA::SecureMessage::ErrorCertKeyMismatch:
        return "certificate and private key don't match";
    case QCA::SecureMessage::ErrorPassphrase:
        return "passphrase was either wrong or not provided";
    case QCA::SecureMessage::ErrorFormat:
        return "input format was bad";
    case QCA::SecureMessage::ErrorSignerExpired:
        return "signing key is expired";
    case QCA::SecureMessage::ErrorSignerInvalid:
        return "signing key is invalid in some way";
    case QCA::SecureMessage::ErrorEncryptExpired:
        return "encrypting key is expired";
    case QCA::SecureMessage::ErrorEncryptUntrusted:
        return "encrypting key is untrusted";
    case QCA::SecureMessage::ErrorEncryptInvalid:
        return "encrypting key is invalid in some way";
    case QCA::SecureMessage::ErrorNeedCard:
        return "pgp card is missing";
    default:
        return "other error";
    }
}

bool OpenPGPView::verify(const QModelIndex &textIndex, const QModelIndex &sigIndex)
{
    QModelIndex mimePart = textIndex.child(0, Imap::Mailbox::TreeItem::OFFSET_MIME);
    Q_ASSERT(mimePart.isValid());

    QByteArray rawsigned_text = mimePart.data(Imap::Mailbox::RolePartData).toByteArray();
    rawsigned_text += textIndex.data(Imap::Mailbox::RolePartRawData).toByteArray();

    qDebug() << rawsigned_text.data();
    qDebug() << sigIndex.data(Imap::Mailbox::RolePartData).toByteArray();

    QCA::Initializer init;
    QCA::OpenPGP pgp;
    QCA::SecureMessage msg(&pgp);

    if ( !msg.signer().key().isNull() )
        qDebug() << msg.signer().key().pgpPublicKey().userIds();
    //msg.setRecipient(to);
    msg.setFormat(QCA::SecureMessage::Ascii);
    msg.startVerify(sigIndex.data(Imap::Mailbox::RolePartData).toByteArray());
    msg.update(rawsigned_text.data());
    msg.end();
    msg.waitForFinished(2000);

    QLabel *lbl = new QLabel((QWidget*) parent());

    if (!msg.success())
    {
        lbl->setText(tr("Signature verification failed. %1").arg(strerror(msg.errorCode())));
        qDebug() << "failed: " << msg.diagnosticText();
    } else {
        QString signer;
        QCA::KeyStoreManager manager;
        QCA::KeyStoreManager::start();
        manager.waitForBusyFinished();
        QCA::KeyStore store("qca-gnupg", &manager);
        foreach(QCA::KeyStoreEntry entry, store.entryList())
            if ( entry.id() == msg.signer().key().pgpPublicKey().keyId() )
                signer = entry.pgpPublicKey().primaryUserId();
        lbl->setText(tr("Signature with key \"%1\" (%2) successfully verified.").arg(signer).arg(msg.signer().key().pgpPublicKey().keyId()));
    }
    this->layout()->addWidget(lbl);

    return msg.success() && msg.verifySuccess();
}

}
