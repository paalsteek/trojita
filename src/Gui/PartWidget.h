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
#ifndef GUI_PARTWIDGET_H
#define GUI_PARTWIDGET_H

#include <QGroupBox>
#include <QTabWidget>

#include "AbstractPartWidget.h"
#include "MessageView.h"
#include "Gui/PartWalker.h"
#include "UiUtils/PartLoadingOptions.h"

class QModelIndex;

namespace Gui
{

/** @short Message quoting support for multipart/alternative MIME type */
class MultipartAlternativeWidget: public QTabWidget, public AbstractPartWidget
{
    Q_OBJECT
public:
    MultipartAlternativeWidget(QWidget *parent, PartWidgetFactory *factory,
                               const QModelIndex &partIndex,
                               const int recursionDepth, const UiUtils::PartLoadingOptions options);
    virtual QString quoteMe() const;
    virtual void reloadContents();
protected:
    bool eventFilter(QObject *o, QEvent *e);
};

/** @short Base class for handling parts where the structure of the chilren is not known yet */
class AsynchronousPartWidget: public QGroupBox, public AbstractPartWidget
{
    Q_OBJECT
public:
    AsynchronousPartWidget(QWidget *parent, PartWidgetFactory *factory, const QModelIndex &partIndex, const int recursionDepth,
                           const UiUtils::PartLoadingOptions loadingOptions);

protected slots:
    void handleRowsInserted(QModelIndex parent, int row, int column);

protected:
    virtual void buildWidgets() = 0;

    PartWidgetFactory *m_factory;
    QPersistentModelIndex m_partIndex;
    const int m_recursionDepth;
    const UiUtils::PartLoadingOptions m_options;
};

/** @short Message quoting support for multipart/signed MIME type */
class MultipartSignedWidget: public AsynchronousPartWidget
{
    Q_OBJECT
public:
    MultipartSignedWidget(QWidget *parent, PartWidgetFactory *factory,
                          const QModelIndex &partIndex, const int recursionDepth,
                          const UiUtils::PartLoadingOptions loadingOptions);
    virtual QString quoteMe() const;
    virtual void reloadContents();

protected:
    virtual void buildWidgets();
};

/** @short Handling of the fact that the message structure of encrypted message parts is not available yet */
class MultipartEncryptedWidget: public AsynchronousPartWidget
{
    Q_OBJECT
public:
    MultipartEncryptedWidget(QWidget *parent, PartWidgetFactory *factory, const QModelIndex &partIndex, const int recursionDepth,
                             const UiUtils::PartLoadingOptions loadingOptions);
    virtual QString quoteMe() const;
    virtual void reloadContents();

protected:
    virtual void buildWidgets();
};

/** @short Message quoting support for generic multipart/ * */
class GenericMultipartWidget: public QWidget, public AbstractPartWidget
{
    Q_OBJECT
public:
    GenericMultipartWidget(QWidget *parent, PartWidgetFactory *factory,
                           const QModelIndex &partIndex, const int recursionDepth,
                           const UiUtils::PartLoadingOptions loadingOptions);
    virtual QString quoteMe() const;
    virtual void reloadContents();
};

/** @short Message quoting support for generic multipart/ * */
class Message822Widget: public QWidget, public AbstractPartWidget
{
    Q_OBJECT
public:
    Message822Widget(QWidget *parent, PartWidgetFactory *factory,
                     const QModelIndex &partIndex, const int recursionDepth,
                     const UiUtils::PartLoadingOptions loadingOptions);
    virtual QString quoteMe() const;
    virtual void reloadContents();
};


}

#endif // GUI_PARTWIDGET_H
