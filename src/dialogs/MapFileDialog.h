#ifndef MAPFILEDIALOG_H
#define MAPFILEDIALOG_H

#include <QDialog>
#include <memory>
#include "core/Cutter.h"

namespace Ui {
class MapFileDialog;
}

class MapFileDialog : public QDialog
{
    Q_OBJECT

public:
    explicit MapFileDialog(QWidget *parent = nullptr);
    ~MapFileDialog();

private slots:
    void onSelectFileButtonClicked();
    void onButtonBoxAccepted();
    void onButtonBoxRejected();

private:
    std::unique_ptr<Ui::MapFileDialog> ui;
};

#endif // MAPFILEDIALOG_H
