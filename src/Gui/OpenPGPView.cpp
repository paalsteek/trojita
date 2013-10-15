#include "OpenPGPView.h"

#include <QLabel>
#include <QVBoxLayout>

#include <QDebug>

#include "MessageView.h"
#include "Imap/Model/ItemRoles.h"

#include <gpgme.h>

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
        gpgme_ctx_t gpgctx;
        gpgme_error_t err;
        //qDebug() << (char*) gpgme_check_version(NULL);
        gpgme_check_version(NULL); // this has to be called before any other gpgme function
        if ((err = gpgme_new(&gpgctx)) != GPG_ERR_NO_ERROR) {
            //qDebug() << "Couldn't create gpg context:" << gpgme_strerror(err);
            return;
        }

        gpgme_set_armor(gpgctx, 1);
        gpgme_set_protocol(gpgctx, GPGME_PROTOCOL_OpenPGP);

        QByteArray rawsigned_text = mimePart.data(RolePartData).toByteArray();
        rawsigned_text += anotherPart.data(RolePartRawData).toByteArray();

        qDebug() << rawsigned_text.data();
        qDebug() << signaturePart.data(RolePartData).toByteArray();

        gpgme_data_t sig, signed_text;
        if ((err = gpgme_data_new_from_mem(&sig, signaturePart.data(RolePartData).toByteArray(), signaturePart.data(RolePartData).toByteArray().size(), 0)) != GPG_ERR_NO_ERROR) {
            //qDebug() << "Couldn't load signature from mail:" << gpgme_strerror(err);
            return;
        }

        if ((err = gpgme_data_new_from_mem(&signed_text, rawsigned_text.data(), rawsigned_text.size(), 0)) != GPG_ERR_NO_ERROR) {
            //qDebug() << "Couldn't load signed text from mail:" << gpgme_strerror(err);
            return;
        }

        gpgme_data_seek(sig, 0, SEEK_SET);
        gpgme_data_seek(signed_text, 0, SEEK_SET);

        if ((err = gpgme_op_verify(gpgctx, sig, signed_text, NULL)) != GPG_ERR_NO_ERROR) {
            //qDebug() << "GPG signature verification failed:" << gpgme_strerror(err);
            return;
        }

        gpgme_verify_result_t result = gpgme_op_verify_result(gpgctx);
        //qDebug() << result->signatures->summary;

        QString sigstate;
        switch(result->signatures->summary)
        {
        case GPGME_SIGSUM_VALID:
            sigstate = "valid";
            break;
        case GPGME_SIGSUM_GREEN:
            sigstate =  "green";
            break;
        case GPGME_SIGSUM_RED:
            sigstate =  "red";
            break;
        case GPGME_SIGSUM_KEY_REVOKED:
            sigstate =  "key revoked";
            break;
        case GPGME_SIGSUM_KEY_EXPIRED:
            sigstate =  "key expired";
            break;
        case GPGME_SIGSUM_SIG_EXPIRED:
            sigstate =  "signature expired";
            break;
        case GPGME_SIGSUM_KEY_MISSING:
            sigstate =  "key missing";
            break;
        case GPGME_SIGSUM_CRL_MISSING:
            sigstate =  "crl missing";
            break;
        case GPGME_SIGSUM_CRL_TOO_OLD:
            sigstate =  "crl too old";
            break;
        case GPGME_SIGSUM_BAD_POLICY:
            sigstate =  "bad policy";
            break;
        case GPGME_SIGSUM_SYS_ERROR:
            sigstate =  "system error";
            break;
        default:
            sigstate =  "unkown";
        }

        gpgme_key_t key;
        gpgme_get_key(gpgctx, result->signatures->fpr, &key, 0);

        QLabel *siglbl = new QLabel(tr("%1 from \"%2 <%3>\" with fingerprint %4").arg(gpgme_strerror(result->signatures->status)).arg(key->uids->name).arg(key->uids->email).arg(result->signatures->fpr), this);

        gpgme_data_release(sig);
        gpgme_data_release(signed_text);
        gpgme_release(gpgctx);
        Q_ASSERT(signaturePart.isValid());
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
