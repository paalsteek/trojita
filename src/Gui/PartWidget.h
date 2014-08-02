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
#include "PartWidgetFactory.h"

class QModelIndex;

namespace Gui
{
class EnvelopeView;
class PartWidgetFactory;

class AbstractMultipartWidget: public AbstractPartWidget
{
public:
    AbstractMultipartWidget(PartWidgetFactory *factory, const QModelIndex &partIndex,
                               const int recursionDepth, const PartWidgetFactory::PartLoadingOptions options);

protected:
    PartWidgetFactory *m_factory;
    const QPersistentModelIndex m_partIndex;
    const int m_recursionDepth;
    const PartWidgetFactory::PartLoadingOptions m_loadingOptions;

};

/** @short Message quoting support for multipart/alternative MIME type */
class MultipartAlternativeWidget: public QTabWidget, public AbstractMultipartWidget
{
    Q_OBJECT
public:
    MultipartAlternativeWidget(QWidget *parent, PartWidgetFactory *factory, const QModelIndex &partIndex,
                               const int recursionDepth, const PartWidgetFactory::PartLoadingOptions options);
    virtual QString quoteMe() const;
    virtual void reloadContents();

protected:
    bool eventFilter(QObject *o, QEvent *e);
};

/** @short Message quoting support for multipart/signed MIME type */
class MultipartSignedWidget: public QGroupBox, public AbstractMultipartWidget
{
    Q_OBJECT
public:
    MultipartSignedWidget(QWidget *parent, PartWidgetFactory *factory, const QModelIndex &partIndex, const int recursionDepth,
                          const PartWidgetFactory::PartLoadingOptions loadingOptions);
    virtual QString quoteMe() const;
    virtual void reloadContents();

protected slots:
    void handleRowsInserted(QModelIndex parent, int first, int last);

protected:
    virtual void rebuildWidgets();
};

/** @short Message quoting support for generic multipart/ * */
class GenericMultipartWidget: public QWidget, public AbstractMultipartWidget
{
    Q_OBJECT
public:
    GenericMultipartWidget(QWidget *parent, PartWidgetFactory *factory, const QModelIndex &partIndex, const int recursionDepth,
                           const PartWidgetFactory::PartLoadingOptions loadingOptions);
    virtual QString quoteMe() const;
    virtual void reloadContents();

protected slots:
    void handleRowsInserted(QModelIndex parent, int first, int last);

protected:
    virtual void rebuildWidgets();
};

/** @short Support for encrypted messages */
class MultipartEncryptedWidget: public GenericMultipartWidget
{
    Q_OBJECT
public:
    MultipartEncryptedWidget(QWidget *parent, PartWidgetFactory *factory, const QModelIndex &partIndex, const int recursionDepth,
                             const PartWidgetFactory::PartLoadingOptions loadingOptions);
};

/** @short Message quoting support for message/822 MIME type */
class Message822Widget: public QWidget, public AbstractMultipartWidget
{
    Q_OBJECT
public:
    Message822Widget(QWidget *parent, PartWidgetFactory *factory, const QModelIndex &partIndex, const int recursionDepth,
                     const PartWidgetFactory::PartLoadingOptions loadingOptions);
    virtual QString quoteMe() const;
    virtual void reloadContents();

private:
    EnvelopeView* m_envelope;
};


}

#endif // GUI_PARTWIDGET_H
