/* Copyright (C) 2006 - 2014 Jan Kundrát <jkt@flaska.net>

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
#include "PartWidget.h"

#include <QLabel>
#include <QModelIndex>
#include <QVBoxLayout>
#include <QTabBar>

#include "EnvelopeView.h"
#include "LoadablePartWidget.h"
#include "MessageView.h"
#include "PartWidgetFactory.h"
#include "Imap/Model/ItemRoles.h"
#include "Imap/Model/MailboxTree.h"

namespace {

    /** @short Unset flags which only make sense for one level of nesting */
    Gui::PartWidgetFactory::PartLoadingOptions filteredForEmbedding(const Gui::PartWidgetFactory::PartLoadingOptions options)
    {
        return options & Gui::PartWidgetFactory::MASK_PROPAGATE_WHEN_EMBEDDING;
    }
}

namespace Gui
{

QString quoteMeHelper(const QObjectList &children)
{
    QStringList res;
    for (QObjectList::const_iterator it = children.begin(); it != children.end(); ++it) {
        const AbstractPartWidget *w = dynamic_cast<const AbstractPartWidget *>(*it);
        if (w)
            res += w->quoteMe();
    }
    return res.join("\n");
}

MultipartAlternativeWidget::MultipartAlternativeWidget(QWidget *parent,
        PartWidgetFactory *factory, const QModelIndex &partIndex,
        const int recursionDepth, const PartWidgetFactory::PartLoadingOptions options):
    QTabWidget(parent)
{
    setContentsMargins(0,0,0,0);

    const bool plaintextIsPreferred = options & PartWidgetFactory::PART_PREFER_PLAINTEXT_OVER_HTML;

    // Which "textual, boring part" should be shown?
    int preferredTextIndex = -1;

    // First loop iteration is used to find out what MIME type to show.
    // Two iterations are needed because we have to know about whether we're shown or hidden when creating child widgets.
    for (int i = 0; i < partIndex.model()->rowCount(partIndex); ++i) {
        QModelIndex anotherPart = partIndex.child(i, 0);
        Q_ASSERT(anotherPart.isValid());

        QString mimeType = anotherPart.data(Imap::Mailbox::RolePartMimeType).toString();

        const bool isPlainText = mimeType == QLatin1String("text/plain");
        const bool isHtml = mimeType == QLatin1String("text/html");

        // At first, check whether this is one of the textual parts which we like
        if (isPlainText && plaintextIsPreferred) {
            preferredTextIndex = i;
        } else if (isHtml && !plaintextIsPreferred) {
            preferredTextIndex = i;
        }
    }

    // Show that part which is "the most important". The choice is usually between text/plain and text/html, one of them will win,
    // depending on the user's preferences.  If there are additional parts, the user will be alerted about them later on.
    // As usual, the later parts win in general.
    int preferredIndex = preferredTextIndex == -1 ? partIndex.model()->rowCount(partIndex) - 1 : preferredTextIndex;

    // The second loop actually creates the widgets
    for (int i = 0; i < partIndex.model()->rowCount(partIndex); ++i) {
        QModelIndex anotherPart = partIndex.child(i, 0);
        Q_ASSERT(anotherPart.isValid());
        // TODO: This is actually not perfect, the preferred part of a multipart/alternative
        // which is nested as a non-preferred part of another multipart/alternative actually gets loaded here.
        // I can live with that.
        QWidget *item = factory->create(anotherPart, recursionDepth + 1,
                                        filteredForEmbedding(i == preferredIndex ?
                                            options :
                                            options | PartWidgetFactory::PART_IS_HIDDEN));
        QString mimeType = anotherPart.data(Imap::Mailbox::RolePartMimeType).toString();

        const bool isPlainText = mimeType == QLatin1String("text/plain");
        const bool isHtml = mimeType == QLatin1String("text/html");

        if (isPlainText) {
            //: Translators: use something very short, perhaps even "text". Don't describe this as "Clear text".
            //: This string is used as a caption of a tab showing the plaintext part of a mail which is e.g.
            //: sent in both plaintext and HTML formats.
            mimeType = tr("Plaintext");
        } else if (isHtml) {
            //: Translators: caption of the tab which shows a HTML version of the mail. Use some short, compact text here.
            mimeType = tr("HTML");
        }

        addTab(item, mimeType);

        // Bug 332950: some items nested within a multipart/alternative message are not exactly an alternative.
        // One such example is a text/calendar right next to a text/html and a text/plain.
        if (!isPlainText && !isHtml) {
            // Unfortunately we cannot change the tab background with current Qt (Q1 2014),
            // see https://bugreports.qt-project.org/browse/QTBUG-840 for details
            setTabIcon(i, QIcon::fromTheme(QLatin1String("emblem-important")));
        }
    }

    setCurrentIndex(preferredIndex);

    tabBar()->installEventFilter(this);
}

QString MultipartAlternativeWidget::quoteMe() const
{
    const AbstractPartWidget *w = dynamic_cast<const AbstractPartWidget *>(currentWidget());
    return w ? w->quoteMe() : QString();
}

void MultipartAlternativeWidget::reloadContents()
{
    if (count()) {
        for (int i = 0; i < count(); ++i) {
            AbstractPartWidget *w = dynamic_cast<AbstractPartWidget *>(widget(i));
            if (w) {
                w->reloadContents();
            }
        }
    }
}

bool MultipartAlternativeWidget::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::Wheel && qobject_cast<QTabBar*>(o)) { // don't alter part while wheeling
        e->ignore();
        return true;
    }
    return false;
}

MultipartSignedWidget::MultipartSignedWidget(QWidget *parent,
        PartWidgetFactory *factory, const QModelIndex &partIndex,
        const int recursionDepth, const PartWidgetFactory::PartLoadingOptions options):
    QGroupBox(tr("Signed Message"), parent)
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
    }
}

QString MultipartSignedWidget::quoteMe() const
{
    return quoteMeHelper(children());
}

GenericMultipartWidget::GenericMultipartWidget(QWidget *parent,
        PartWidgetFactory *factory, const QModelIndex &partIndex,
        int recursionDepth, const PartWidgetFactory::PartLoadingOptions options):
    QWidget(parent)
  , m_partIndex(partIndex)
  , m_factory(factory)
  , m_recursionDepth(recursionDepth)
  , m_loadingOptions(options)
{
    connect(partIndex.model(), SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(handleRowsInserted(QModelIndex,int,int)));
    setContentsMargins(0, 0, 0, 0);
    // multipart/mixed or anything else, as mandated by RFC 2046, Section 5.1.3
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    for (int i = 0; i < partIndex.model()->rowCount(partIndex); ++i) {
        using namespace Imap::Mailbox;
        QModelIndex anotherPart = partIndex.child(i, 0);
        Q_ASSERT(anotherPart.isValid()); // guaranteed by the MVC
        QWidget *res = factory->create(anotherPart, recursionDepth + 1, filteredForEmbedding(options));
        layout->addWidget(res);
    }
}

QString GenericMultipartWidget::quoteMe() const
{
    return quoteMeHelper(children());
}

void GenericMultipartWidget::handleRowsInserted(QModelIndex parent, int first, int last)
{
    qDebug() << "GenericMultipartWidget::handleRowsInserted(" << parent << "," << first << "," << last << ")";
    if (parent == m_partIndex) {
        qDebug() << "new data inserted in" << parent.data(Imap::Mailbox::RolePartMimeType);
        QLayout *l = layout();
        delete l;
        QVBoxLayout *layout = new QVBoxLayout(this);
        layout->setSpacing(0);
        for (int i = 0; i < parent.model()->rowCount(parent); ++i) {
            using namespace Imap::Mailbox;
            QModelIndex anotherPart = parent.child(i, 0);
            qDebug() << "Part" << i << ":" << anotherPart.data(Imap::Mailbox::RolePartMimeType);
            Q_ASSERT(anotherPart.isValid()); // guaranteed by the MVC
            QWidget *res = m_factory->create(anotherPart, m_recursionDepth + 1, filteredForEmbedding(m_loadingOptions));
            layout->addWidget(res);
        }
    }
}

Message822Widget::Message822Widget(QWidget *parent,
                                   PartWidgetFactory *factory, const QModelIndex &partIndex,
                                   int recursionDepth, const PartWidgetFactory::PartLoadingOptions options):
    QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setSpacing(0);
    EnvelopeView *envelope = new EnvelopeView(0, factory->messageView());
    envelope->setMessage(partIndex);
    layout->addWidget(envelope);
    for (int i = 0; i < partIndex.model()->rowCount(partIndex); ++i) {
        using namespace Imap::Mailbox;
        QModelIndex anotherPart = partIndex.child(i, 0);
        Q_ASSERT(anotherPart.isValid()); // guaranteed by the MVC
        QWidget *res = factory->create(anotherPart, recursionDepth + 1, filteredForEmbedding(options));
        layout->addWidget(res);
    }
}

QString Message822Widget::quoteMe() const
{
    return quoteMeHelper(children());
}

#define IMPL_RELOAD(CLASS) void CLASS::reloadContents() \
{\
    /*qDebug() << metaObject()->className() << children().size();*/\
    Q_FOREACH( QObject* const obj, children() ) {\
        /*qDebug() << obj->metaObject()->className();*/\
        AbstractPartWidget* w = dynamic_cast<AbstractPartWidget*>( obj );\
        if ( w ) {\
            /*qDebug() << "reloadContents:" << w;*/\
            w->reloadContents();\
        }\
    }\
}

IMPL_RELOAD(MultipartSignedWidget);
IMPL_RELOAD(GenericMultipartWidget);
IMPL_RELOAD(Message822Widget);

#undef IMPL_RELOAD


}


