#ifndef HEAPDOCKWIDGET_H
#define HEAPDOCKWIDGET_H

#include "CutterDockWidget.h"

#include <QDockWidget>

#include <memory>

namespace Ui {
class HeapDockWidget;
}

/**
 * @brief A container widget that serves as the primary interface for heap analysis, providing a
 * flexible layout to switch between different memory allocator implementations.
 */
class HeapDockWidget : public CutterDockWidget
{
    Q_OBJECT

public:
    explicit HeapDockWidget(MainWindow *main);
    ~HeapDockWidget();
private slots:
    void onAllocatorSelected(int index);

private:
    enum Allocator : ut8 { Glibc = 0, AllocatorCount };
    std::unique_ptr<Ui::HeapDockWidget> ui;
    MainWindow *main;
    QWidget *currentHeapWidget = nullptr;
};

#endif // HEAPDOCKWIDGET_H
