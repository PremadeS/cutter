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
