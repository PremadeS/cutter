#ifndef SEARCHABLETEXTEDIT_H
#define SEARCHABLETEXTEDIT_H

#include <QPlainTextEdit>

// TODO: I think better to put in common/

class SearchableTextEdit : public QPlainTextEdit
{
    Q_OBJECT

public:
    explicit SearchableTextEdit(QWidget *parent = nullptr);

    // returns <index, count>
    QPair<int, int> search(const QString &str, QTextDocument::FindFlags flags, bool isRegex);
    void clearSearch();

    // returns new index
    int findNext();
    int findPrev();
    int findLast();

protected:
    void resizeEvent(QResizeEvent *event) override;

public slots:
    void highlightSearches();

private:
    int currentIndex;

    QList<QTextCursor> searchCursors;
    int searchedMatchLen;

    void handleMatch(const QTextCursor &currentCursor, const QTextCursor &originalCursor);
    void scrollAndHighlight();
};

#endif // SEARCHABLETEXTEDIT_H
