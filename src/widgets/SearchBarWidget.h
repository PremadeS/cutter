#ifndef SEARCHBARWIDGET_H
#define SEARCHBARWIDGET_H

#include <QWidget>
#include <QSizeGrip>
#include <memory>

namespace Ui {
class SearchBarWidget;
}

/**
 * @brief Small grip to let users resize the search bar for the left
 */
class SearchBarSizeGrip : public QSizeGrip
{
public:
    explicit SearchBarSizeGrip(QWidget *parent = nullptr);

protected:
    /**
     * @brief Changes the cursor icon to indicate search bar is expandable in width
     */
    void moveEvent(QMoveEvent *moveEvent) override;
    void paintEvent(QPaintEvent *event) override;
};

/**
 * @brief The main search bar widget for finding text
 */
class SearchBarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SearchBarWidget(QWidget *parent = nullptr);
    ~SearchBarWidget();

    void setTotalCount(int count);
    void setCurrentIndex(int index);
    void setRange(int index, int count);

    int totalCount() const;
    int currentIndex() const;
    QString text() const;
    int options() const;

    void updateLabel();
    void clear();

public slots:
    void showSearchBar();
    void hideSearchBar();
    void selectText();

protected:
    void paintEvent(QPaintEvent *event) override;

signals:
    void searchChanged(const QString &text, int options);
    void findNextTriggered();
    void findPrevTriggered();
    void findLastTriggered();
    void hideTriggered();
    void showTriggered();

private:
    std::unique_ptr<Ui::SearchBarWidget> ui;
    int mIndex = 0;
    int mCount = 0;

    QAction *mCaseSensitiveAction;
    QAction *mWholeWordsAction;
    QAction *mRegExpAction;
    QAction *mHighlightMatchesAction;
};

#endif // SEARCHBARWIDGET_H
