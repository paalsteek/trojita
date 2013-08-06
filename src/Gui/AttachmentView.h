/* Copyright (C) 2006 - 2013 Jan Kundrát <jkt@flaska.net>

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
#ifndef ATTACHMENTVIEW_H
#define ATTACHMENTVIEW_H

#include <QModelIndex>
#include <QWidget>


class QNetworkReply;
class QPushButton;
class QTemporaryFile;
class QToolButton;

namespace Imap
{
namespace Network
{
class FileDownloadManager;
class MsgPartNetAccessManager;
}
}

namespace Gui
{

class MessageView;

/** @short Widget depicting an attachment

  This widget provides a graphical representation about an e-mail attachment,
  that is, an interactive item which shows some basic information like the MIME
  type of the body part and the download button.  It also includes code for
  handling the actual download.
*/
class AttachmentView : public QWidget
{
    Q_OBJECT
public:
    AttachmentView(QWidget *parent, Imap::Network::MsgPartNetAccessManager *manager, const QModelIndex &m_partIndex,
                   MessageView *messageView);
protected:
    virtual void mousePressEvent(QMouseEvent *event);
private slots:
    void slotDownloadAttachment();
    void slotOpenAttachment();

    void slotDeleteTemporaryFile(); // XXX: better name?
    void slotFileNameRequestedOnOpen(QString *fileName);
    void slotFileNameRequested(QString *fileName);
    void slotTransferError(const QString &errorString);
    void slotTransferStarted();
    void slotTransferSucceeded();

private:
    QPersistentModelIndex m_partIndex;
    QToolButton *m_downloadButton;

    MessageView *m_messageView;

    QAction *m_downloadAttachment;
    QAction *m_openAttachment;

    Imap::Network::MsgPartNetAccessManager *m_netAccess;
    Imap::Network::FileDownloadManager *m_openingManager;

    QTemporaryFile *m_tmpFile;

    AttachmentView(const AttachmentView &); // don't implement
    AttachmentView &operator=(const AttachmentView &); // don't implement
};

}

#endif // ATTACHMENTVIEW_H
