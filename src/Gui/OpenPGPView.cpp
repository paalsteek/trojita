#include "OpenPGPView.h"

#include <QLabel>
#include <QVBoxLayout>

#include <QtCrypto/QtCrypto>

#include <QDebug>

#include "MessageView.h"
#include "Imap/Model/ItemRoles.h"

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
    : QGroupBox(tr("OpenPGP signed message"), parent)
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
        QModelIndex mimePart = partIndex.child(0,0).child(0, 3); //OFFSET_MIME=3
        Q_ASSERT(anotherPart.isValid()); // guaranteed by the MVC
        Q_ASSERT(mimePart.isValid());
        layout->addWidget(factory->create(anotherPart, recursionDepth + 1, filteredForEmbedding(options)));
        //qDebug() << "mimePart:" << mimePart.data(RolePartData);
        QModelIndex signaturePart = partIndex.child(1, 0);
        Q_ASSERT(signaturePart.isValid());

        QByteArray rawsigned_text = mimePart.data(RolePartData).toByteArray();
        rawsigned_text += anotherPart.data(RolePartRawData).toByteArray();

        qDebug() << rawsigned_text.data();
        qDebug() << signaturePart.data(RolePartData).toByteArray();

        QCA::Initializer init;
        QCA::OpenPGP pgp;
        QCA::SecureMessage msg(&pgp);

        //msg.setRecipient(to);
        msg.setFormat(QCA::SecureMessage::Ascii);
        msg.startVerify(signaturePart.data(RolePartData).toByteArray());
        msg.update(rawsigned_text.data());
        msg.end();
        msg.waitForFinished(2000);

        QLabel *siglbl = new QLabel(tr(msg.verifySuccess()?"Success":"Failure"), this);

        layout->addWidget(siglbl);
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

void OpenPGPView::handleDataChanged(QModelIndex topLeft, QModelIndex bottomDown)
{

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

}
