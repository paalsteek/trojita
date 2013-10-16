#ifndef OPENPGPVIEW_H
#define OPENPGPVIEW_H

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
    OpenPGPView(QWidget *parent, PartWidgetFactory *factory, const QModelIndex &partIndex, const int recursionDepth,
                const PartWidgetFactory::PartLoadingOptions options);
    virtual QString quoteMe() const;
    virtual void reloadContents();
    bool verify(const QModelIndex &textIndex, const QModelIndex &sigIndex);

private slots:
    void handleDataChanged(const QModelIndex &topLeft, const QModelIndex &bottomDown);

private:
    const Imap::Mailbox::Model *m_model;
    QModelIndex m_partIndex;
};
}

#endif // OPENPGPVIEW_H
