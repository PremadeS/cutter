#ifndef SEARCHABLEDOCKWIDGET_H
#define SEARCHABLEDOCKWIDGET_H

#include "CutterDockWidget.h"
#include "CutterSearchable.h"

class SearchBarWidget;

class CUTTER_EXPORT SearchableDockWidget : public CutterDockWidget, public CutterSearchable
{
    Q_OBJECT

public:
    explicit SearchableDockWidget(MainWindow *parent);

protected:
    // maybe m_searchBar???
    SearchBarWidget *searchBar;

    void resizeEvent(QResizeEvent *event) override;
    int searchHPadding() const override;
    int searchVPadding() const override;

private:
};

#endif // SEARCHABLEDOCKWIDGET_H
