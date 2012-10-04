/* Copyright (C) 2006 - 2012 Jan Kundrát <jkt@flaska.net>

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or version 3 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; see the file COPYING.  If not, write to
   the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.
*/


#include "Fake_ListChildMailboxesTask.h"
#include "GetAnyConnectionTask.h"
#include "Model.h"
#include "MailboxTree.h"
#include "TaskFactory.h"

namespace Imap
{
namespace Mailbox
{


Fake_ListChildMailboxesTask::Fake_ListChildMailboxesTask(Model *model, const QModelIndex &mailbox):
    ListChildMailboxesTask(model, mailbox)
{
    TreeItemMailbox *mailboxPtr = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailbox.internalPointer()));
    Q_ASSERT(mailboxPtr);
}

void Fake_ListChildMailboxesTask::perform()
{
    parser = conn->parser;
    markAsActiveTask();

    IMAP_TASK_CHECK_ABORT_DIE;

    TreeItemMailbox *mailbox = dynamic_cast<TreeItemMailbox *>(static_cast<TreeItem *>(mailboxIndex.internalPointer()));
    Q_ASSERT(mailbox);
    parser = conn->parser;
    QList<Responses::List> &listResponses = model->accessParser(parser).listResponses;
    Q_ASSERT(listResponses.isEmpty());
    TestingTaskFactory *factory = dynamic_cast<TestingTaskFactory *>(model->m_taskFactory.get());
    Q_ASSERT(factory);
    for (QMap<QString, QStringList>::const_iterator it = factory->fakeListChildMailboxesMap.constBegin();
         it != factory->fakeListChildMailboxesMap.constEnd(); ++it) {
        if (it.key() != mailbox->mailbox())
            continue;
        for (QStringList::const_iterator childIt = it->begin(); childIt != it->end(); ++childIt) {
            QString childMailbox = mailbox->mailbox().isEmpty() ? *childIt : QString::fromUtf8("%1^%2").arg(mailbox->mailbox(), *childIt);
            listResponses.append(Responses::List(Responses::LIST, QStringList(), QLatin1String("^"), childMailbox, QMap<QByteArray, QVariant>()));
        }
    }
    model->finalizeList(parser, mailbox);
    _completed();
}

bool Fake_ListChildMailboxesTask::handleStateHelper(const Imap::Responses::State *const resp)
{
    // This is a fake task inheriting from the "real one", so we have to reimplement functions which do real work with stubs
    Q_UNUSED(resp);
    return false;
}


}
}
