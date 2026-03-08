#include "SearchHighlight.h"
#include "SelectionHighlight.h"
#include "Configuration.h"
#include "CutterSearchable.h"

#include <QTextDocument>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QRegularExpression>

int SearchHighlight::updateSearchHighlights(QPlainTextEdit *textEdit, const QString &word,
                                            int options)
{
    if (!textEdit) {
        return 0;
    }

    if (word.isEmpty()) {
        textEdit->setExtraSelections({});
        return 0;
    }

    QTextDocument::FindFlags flags;
    flags = (options & SearchOption::WholeWords) ? (flags | QTextDocument::FindWholeWords) : flags;
    flags = (options & SearchOption::CaseSensitive) ? (flags | QTextDocument::FindCaseSensitively)
                                                    : flags;

    QColor highlightColor =
            (options & SearchOption::HighlightMatches) ? ConfigColor("wordHighlightBg") : QColor();

    auto selections = createMatchSelections(textEdit->document(), word, highlightColor, flags,
                                            options & SearchOption::RegExp);

    textEdit->setExtraSelections(selections);
    return selections.size();
}

void SearchHighlight::moveFocus(QPlainTextEdit *textEdit, int toIndex, int fromIndex,
                                bool setFromBg, bool setCursor)
{
    // TODO: change the bg color aswell
    auto selections = textEdit->extraSelections();
    if (fromIndex >= 0 && fromIndex < selections.size()) {
        selections[fromIndex].format.setBackground(setFromBg ? ConfigColor("wordHighlightBg")
                                                             : Qt::transparent);
    }

    if (toIndex >= 0 && toIndex < selections.size()) {
        selections[toIndex].format.setBackground(Qt::magenta); // TODO: change this color

        if (setCursor) {
            QTextCursor focusCursor = selections[toIndex].cursor;
            focusCursor.clearSelection();
            textEdit->setTextCursor(focusCursor);
        }
        textEdit->setExtraSelections(selections);
    }
}

int SearchHighlight::search(QPlainTextEdit *textEdit, const QString &str, int &index)
{
    if (!textEdit) {
        return {};
    }

    index = 0;

    QList<int> positions;

    // we will search relative to this cursor
    QTextCursor originalCursor = textEdit->textCursor();
    bool match = false;
    int matchLen = 0;

    // start from the top
    QTextDocument *doc = textEdit->document();
    QTextCursor cursor(doc);

    while (!cursor.isNull() && !cursor.atEnd()) {
        cursor = doc->find(str, cursor);

        if (!cursor.isNull()) {
            if (!match && cursor.position() > originalCursor.position()) {
                index = positions.size();
                match = true;
            }
            // TODO: avoid repeating this
            matchLen = cursor.selectionEnd() - cursor.selectionStart();
            positions.append(cursor.selectionStart());
        }
    }

    if (!match) {
        index = (positions.isEmpty()) ? -1 : 0;
    }

    if (index >= 0) {
        // specifically for scrolling to the desired pos
        QTextCursor scrollCursor(doc);
        scrollCursor.setPosition(positions[index]);
        textEdit->setTextCursor(scrollCursor);
    }

    highlightSearch(textEdit, positions, matchLen, index);
    return positions.size();
}

void SearchHighlight::highlightSearch(QPlainTextEdit *textEdit, const QList<int> &positions,
                                      const int strLen, int &index)
{

    if (index < 0) {
        textEdit->setExtraSelections({});
        return;
    }

    // TODO: check these -= and +=
    QPoint startPoint = QPoint(0, 0);
    auto startPos = textEdit->cursorForPosition(startPoint).position();
    startPos -= strLen;

    QPoint endPoint = QPoint(textEdit->geometry().width(), textEdit->geometry().height());
    auto endPos = textEdit->cursorForPosition(endPoint).position();
    endPos += strLen;

    QList<QTextEdit::ExtraSelection> selections;
    for (int i = 0; i < positions.size(); ++i) {
        int pos = positions[i];
        QTextCursor cursor(textEdit->document());
        cursor.setPosition(pos);
        cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor, strLen);
        if (i == index) {
            QTextEdit::ExtraSelection selection;
            selection.format.setBackground(Qt::magenta);
            selection.cursor = cursor;
            selections.append(selection);
        } else if (pos >= startPos && pos < endPos) {
            QTextEdit::ExtraSelection selection;
            selection.format.setBackground(Qt::red);
            selection.cursor = cursor;
            selections.append(selection);
        }
    }
    textEdit->setExtraSelections(selections);
}
