#include "SearchableTextEdit.h"
#include "CutterSearchable.h"
#include "Configuration.h"

#include <QRegularExpression>

SearchableTextEdit::SearchableTextEdit(QWidget *parent) : QPlainTextEdit(parent) {}

QPair<int, int> SearchableTextEdit::search(const QString &str, QTextDocument::FindFlags flags,
                                           bool isRegex)
{
    searchCursors.clear();
    currentIndex = -1;
    searchedMatchLen = -1;

    if (str.isEmpty()) {
        this->setExtraSelections({});
        return { 0, 0 };
    }

    // we will search relative to this cursor
    QTextCursor originalCursor = this->textCursor();

    // start from the top
    QTextDocument *doc = this->document();
    QTextCursor cursor(doc);

    if (isRegex) {
        const QRegularExpression regex(str);
        while (!cursor.isNull() && !cursor.atEnd()) {
            cursor = doc->find(regex, cursor, flags);
            handleMatch(cursor, originalCursor);
        }
    } else {
        while (!cursor.isNull() && !cursor.atEnd()) {
            cursor = doc->find(str, cursor, flags);
            handleMatch(cursor, originalCursor);
        }
    }

    // wrap around
    if (currentIndex < 0 && !searchCursors.isEmpty()) {
        currentIndex = 0;
    }

    if (currentIndex >= 0) {
        scrollAndHighlight();
    }

    return { currentIndex, searchCursors.size() };
}

void SearchableTextEdit::handleMatch(const QTextCursor &currentCursor,
                                     const QTextCursor &originalCursor)
{
    if (!currentCursor.isNull()) {

        if (this->currentIndex < 0
            && currentCursor.selectionStart() >= originalCursor.selectionStart()) {
            this->currentIndex = searchCursors.size();
        }

        searchCursors.append(currentCursor);
    }
}

void SearchableTextEdit::clearSearch()
{
    this->setExtraSelections({});
    searchCursors.clear();
}

int SearchableTextEdit::findNext()
{
    currentIndex = (currentIndex + 1) % searchCursors.size();
    scrollAndHighlight();

    return currentIndex;
}

int SearchableTextEdit::findPrev()
{
    int count = searchCursors.size();
    currentIndex = (currentIndex - 1 + count) % count;
    scrollAndHighlight();

    return currentIndex;
}

int SearchableTextEdit::findLast()
{
    currentIndex = searchCursors.size() - 1;
    scrollAndHighlight();

    return currentIndex;
}

void SearchableTextEdit::resizeEvent(QResizeEvent *event)
{
    highlightSearches();
    QPlainTextEdit::resizeEvent(event);
}

void SearchableTextEdit::highlightSearches()
{
    if (currentIndex < 0) {
        this->setExtraSelections({});
        return;
    }

    QPoint startPoint = QPoint(0, 0);
    int startPos = this->cursorForPosition(startPoint).position();
    startPos -= searchedMatchLen;
    startPos = startPos >= 0 ? startPos : 0;

    QPoint endPoint = QPoint(this->geometry().width(), this->geometry().height());
    auto endCursor = this->cursorForPosition(endPoint);
    int endPos = endCursor.position();
    endPos += searchedMatchLen;
    endCursor.movePosition(QTextCursor::End);
    endPos = endPos <= endCursor.position() ? endPos : endCursor.position();

    QList<QTextEdit::ExtraSelection> selections;
    for (int i = 0; i < searchCursors.size(); ++i) {
        const QTextCursor cursor = searchCursors[i];
        int pos = cursor.selectionStart();
        if (i == currentIndex) {
            QTextEdit::ExtraSelection selection;
            selection.format.setBackground(ConfigColor("searchCurrent"));
            selection.cursor = cursor;
            selections.append(selection);
        } else if (pos >= startPos && pos < endPos) {
            QTextEdit::ExtraSelection selection;
            selection.format.setBackground(ConfigColor("searchHighlight"));
            selection.cursor = cursor;
            selections.append(selection);
        }
    }
    this->setExtraSelections(selections);
}

void SearchableTextEdit::scrollAndHighlight()
{
    QTextCursor scrollCursor = searchCursors[currentIndex];
    scrollCursor.setPosition(scrollCursor.selectionStart());
    scrollCursor.clearSelection();
    this->setTextCursor(scrollCursor);

    highlightSearches();
}
