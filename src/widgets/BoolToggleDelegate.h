#ifndef BOOLTOGGGLEDELEGATE_H
#define BOOLTOGGGLEDELEGATE_H

#include "core/CutterCommon.h"

#include <QStyledItemDelegate>

/**
 * @brief Delegate that toggles boolean model values on double click instead of opening an
 * editor
 */
class CUTTER_EXPORT BoolTogggleDelegate : public QStyledItemDelegate
{
public:
    BoolTogggleDelegate(QObject *parent = nullptr);

    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &option,
                          const QModelIndex &index) const override;

    bool editorEvent(QEvent *event, QAbstractItemModel *model, const QStyleOptionViewItem &option,
                     const QModelIndex &index) override;
};

#endif // BOOLTOGGGLEDELEGATE_H
