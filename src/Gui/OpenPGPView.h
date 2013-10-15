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
private slots:
    void handleDataChanged(QModelIndex topLeft, QModelIndex bottomDown);
};
}

#endif // OPENPGPVIEW_H
