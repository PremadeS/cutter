#include "SearchableTextEdit.h"
#include "CutterSearchable.h"
#include "Configuration.h"

#include <QRegularExpression>

SearchableTextEdit::SearchableTextEdit(QWidget *parent) : QPlainTextEdit(parent) {}

QPair<int, int> SearchableTextEdit::search(const QString &string, int options)
{
    mSearchResults.clear();
    mCurrentIndex = -1;
    mHighlightMatches = options & SearchOption::HighlightMatches;

    if (string.isEmpty()) {
        clearSearch();
        return { 0, 0 };
    }

    QTextDocument::FindFlags flags = {};
    flags = options & SearchOption::CaseSensitive ? (flags | QTextDocument::FindCaseSensitively)
                                                  : flags;
    flags = options & SearchOption::WholeWords ? (flags | QTextDocument::FindWholeWords) : flags;

    // we will search relative to this cursor
    const QTextCursor originalCursor = this->textCursor();

    // start from the top
    QTextDocument *doc = this->document();
    QTextCursor cursor(doc);

    if (options & SearchOption::RegExp) {
        const QRegularExpression regex(string);
        while (!cursor.isNull() && !cursor.atEnd()) {
            cursor = doc->find(regex, cursor, flags);

            if (cursor.hasSelection()) {
                handleMatch(cursor, originalCursor);
            } else {
                cursor.movePosition(QTextCursor::NextCharacter); // avoid infinite loop
            }
        }
    } else {
        while (!cursor.isNull() && !cursor.atEnd()) {
            cursor = doc->find(string, cursor, flags);
            handleMatch(cursor, originalCursor);
        }
    }

    // wrap around
    if (mCurrentIndex < 0 && !mSearchResults.isEmpty()) {
        mCurrentIndex = 0;
    }

    scrollToCurrentIndex();
    highlightMatches();

    return { mCurrentIndex, mSearchResults.size() };
}

void SearchableTextEdit::handleMatch(const QTextCursor &currentCursor,
                                     const QTextCursor &originalCursor)
{
    if (!currentCursor.isNull()) {

        if (mCurrentIndex < 0 && currentCursor.selectionEnd() >= originalCursor.selectionStart()) {
            mCurrentIndex = mSearchResults.size();
        }

        const int len = currentCursor.selectionEnd() - currentCursor.selectionStart();
        mSearchResults.append(SearchResult { currentCursor.selectionStart(), len });
    }
}

void SearchableTextEdit::clearSearch()
{
    this->setExtraSelections({});
    mSearchResults.clear();
}

int SearchableTextEdit::findNext()
{
    if (mSearchResults.isEmpty()) {
        return 0;
    }

    mCurrentIndex = (mCurrentIndex + 1) % mSearchResults.size();

    scrollToCurrentIndex();
    highlightMatches();

    return mCurrentIndex;
}

int SearchableTextEdit::findPrev()
{
    if (mSearchResults.isEmpty()) {
        return 0;
    }

    const int count = mSearchResults.size();
    mCurrentIndex = (mCurrentIndex - 1 + count) % count;
    scrollToCurrentIndex();
    highlightMatches();

    return mCurrentIndex;
}

int SearchableTextEdit::findLast()
{
    if (mSearchResults.isEmpty()) {
        return 0;
    }

    mCurrentIndex = mSearchResults.size() - 1;

    scrollToCurrentIndex();
    highlightMatches();

    return mCurrentIndex;
}

void SearchableTextEdit::resizeEvent(QResizeEvent *event)
{
    highlightMatches();
    QPlainTextEdit::resizeEvent(event);
}

void SearchableTextEdit::highlightMatches()
{
    if (mCurrentIndex < 0 || mCurrentIndex >= mSearchResults.size()) {
        this->setExtraSelections({});
        return;
    }

    QTextCursor cursor(this->document());

    if (!mHighlightMatches) {
        mapCursorToResult(cursor, mSearchResults[mCurrentIndex]);
        QTextEdit::ExtraSelection selection;
        selection.format.setBackground(ConfigColor("searchCurrent"));
        selection.cursor = cursor;
        this->setExtraSelections({ selection });
        return;
    }

    const QPoint startPoint = QPoint(0, 0);
    const int startPos = this->cursorForPosition(startPoint).position();

    const QPoint endPoint = QPoint(this->geometry().width(), this->geometry().height());
    auto endCursor = this->cursorForPosition(endPoint);
    const int endPos = endCursor.position();
    endCursor.movePosition(QTextCursor::End);
    const int maxPos = endCursor.position();

    QList<QTextEdit::ExtraSelection> selections;
    for (int i = 0; i < mSearchResults.size(); ++i) {
        const auto res = mSearchResults[i];
        mapCursorToResult(cursor, mSearchResults[i]);
        const int sPos = std::max(startPos - res.length, 0);
        const int ePos = std::min(endPos + res.length, maxPos);
        if (i == mCurrentIndex) {
            QTextEdit::ExtraSelection selection;
            selection.format.setBackground(ConfigColor("searchCurrent"));
            selection.cursor = cursor;
            selections.append(selection);
        } else if (cursor.selectionStart() >= sPos && cursor.selectionEnd() <= ePos) {
            // only highlight visible matches
            QTextEdit::ExtraSelection selection;
            selection.format.setBackground(ConfigColor("searchHighlight"));
            selection.cursor = cursor;
            selections.append(selection);
        }
    }
    this->setExtraSelections(selections);
}

void SearchableTextEdit::scrollToCurrentIndex()
{
    if (mCurrentIndex < 0 || mCurrentIndex >= mSearchResults.size()) {
        return;
    }

    QTextCursor scrollCursor(this->document());
    mapCursorToResult(scrollCursor, mSearchResults[mCurrentIndex]);
    scrollCursor.setPosition(scrollCursor.selectionStart());
    scrollCursor.clearSelection();
    this->setTextCursor(scrollCursor);
}
