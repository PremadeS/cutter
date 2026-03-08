#ifndef SEARCHBARWIDGET_H
#define SEARCHBARWIDGET_H

#include <QWidget>
#include <QSizeGrip>
#include <QTextDocument>

namespace Ui {
class SearchBarWidget;
}

class SearchBarSizeGrip : public QSizeGrip
{
public:
    explicit SearchBarSizeGrip(QWidget *parent = nullptr);

protected:
    // TODO DOCS, CHANGE THESE
    /**
     * QSizeGrip sets the cursor inside moveEvent()
     * We override this function to set our desired cursor
     */
    void moveEvent(QMoveEvent *moveEvent) override;
    void paintEvent(QPaintEvent *event) override;
};

// TODO: ADD THE WIDGET TO FOCUS ON RAISE
class SearchBarWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SearchBarWidget(QWidget *parent = nullptr);
    ~SearchBarWidget();

    // TODO: DOCSSSSSSSSSSS
    void setTotalCount(int count);
    void setCurrentIndex(int index);
    void setRange(int index, int count);

    int totalCount() const;
    int currentIndex() const;
    int searchOptions() const;
    QTextDocument::FindFlags documentFlags() const;
    bool isRegex() const;

    QString text() const;

    void updateLabel();
    void clear();

public slots:
    void showSearchBar();
    void hideSearchBar();
    void selectText();

protected:
    void paintEvent(QPaintEvent *event) override;

signals:
    void textChanged(const QString &text);
    void findNextTriggered();
    void findPrevTriggered();
    void findLastTriggered();
    void hideTriggered();
    void showTriggered();

private:
    std::unique_ptr<Ui::SearchBarWidget> ui;
    int m_index = 0;
    int m_count = 0;

    QAction *m_caseSensitiveAction;
    QAction *m_wholeWordsAction;
    QAction *m_regExpAction;
    QAction *m_highlightMatchesAction;

    void chooseThemeIcons();
};

#endif // SEARCHBARWIDGET_H
