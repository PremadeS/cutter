
#include "SelectionHighlight.h"
#include "Configuration.h"
#include "Colors.h"
#include "CutterSearchable.h"

#include <QList>
#include <QTextEdit>
#include <QColor>
#include <QTextCursor>
#include <QPlainTextEdit>
#include <QRegularExpression>

QList<QTextEdit::ExtraSelection> createSameWordsSelections(QPlainTextEdit *textEdit,
                                                           const QString &word)
{
    QList<QTextEdit::ExtraSelection> selections;
    QTextEdit::ExtraSelection highlightSelection;
    QTextDocument *document = textEdit->document();
    QColor highlightWordBgColor = ConfigColor("wordHighlightBg");
    QColor highlightWordFgColor = ConfigColor("wordHighlightFg");

    auto applyHighlight = [&] {
        QColor originalColor = highlightSelection.cursor.charFormat().foreground().color();
        highlightSelection.format.setForeground(
                Colors::overlayColor(originalColor, highlightWordFgColor));
        highlightSelection.format.setBackground(highlightWordBgColor);
    };

    if (word.isEmpty()) {
        return QList<QTextEdit::ExtraSelection>();
    }

    highlightSelection.cursor = textEdit->textCursor();

    if (word == "{" || word == "}") {
        int val;
        if (word == "{") {
            val = 0;
        } else {
            val = 1;
        }
        selections.append(highlightSelection);

        while (!highlightSelection.cursor.isNull() && !highlightSelection.cursor.atEnd()) {
            if (word == "{") {
                highlightSelection.cursor =
                        document->find(QRegularExpression("{|}"), highlightSelection.cursor);
            } else {
                highlightSelection.cursor =
                        document->find(QRegularExpression("{|}"), highlightSelection.cursor,
                                       QTextDocument::FindBackward);
            }

            if (!highlightSelection.cursor.isNull()) {
                if (highlightSelection.cursor.selectedText() == word) {
                    val++;
                } else {
                    val--;
                }
                if (val == 0) {
                    applyHighlight();
                    selections.append(highlightSelection);
                    break;
                }
            }
        }
        return selections;
    }

    return createMatchSelections(document, word, highlightWordBgColor,
                                 QTextDocument::FindWholeWords);
}

QTextEdit::ExtraSelection createLineHighlight(const QTextCursor &cursor, QColor highlightColor)
{
    QTextEdit::ExtraSelection highlightSelection;
    highlightSelection.cursor = cursor;
    highlightSelection.format.setBackground(highlightColor);
    highlightSelection.format.setProperty(QTextFormat::FullWidthSelection, true);
    highlightSelection.cursor.clearSelection();
    return highlightSelection;
}

QTextEdit::ExtraSelection createLineHighlightSelection(const QTextCursor &cursor)
{
    QColor highlightColor = ConfigColor("lineHighlight");
    return createLineHighlight(cursor, highlightColor);
}

QTextEdit::ExtraSelection createLineHighlightPC(const QTextCursor &cursor)
{
    QColor highlightColor = ConfigColor("highlightPC");
    return createLineHighlight(cursor, highlightColor);
}

QTextEdit::ExtraSelection createLineHighlightBP(const QTextCursor &cursor)
{
    QColor highlightColor = ConfigColor("gui.breakpoint_background");
    return createLineHighlight(cursor, highlightColor);
}

QList<QTextEdit::ExtraSelection> createMatchSelections(QTextDocument *document, const QString &word,
                                                       const QColor &color,
                                                       QTextDocument::FindFlags flags, bool regex)
{
    QList<QTextEdit::ExtraSelection> selections;
    if (!document || word.isEmpty()) {
        return selections;
    }

    QTextCursor cursor(document);
    QTextEdit::ExtraSelection highlightSelection;
    if (color.isValid()) {
        highlightSelection.format.setBackground(color);
    }

    while (!cursor.isNull() && !cursor.atEnd()) {
        cursor = regex ? document->find(QRegularExpression(word), cursor, flags)
                       : document->find(word, cursor, flags);

        if (!cursor.isNull()) {
            highlightSelection.cursor = cursor;
            selections.append(highlightSelection);
        }
    }

    return selections;
}
