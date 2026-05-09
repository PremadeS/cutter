#include "Highlighter.h"
#include "core/MainWindow.h"

#include <QtGui>

Highlighter::Highlighter(QTextDocument *parent) : QSyntaxHighlighter(parent)
{
    HighlightingRule rule;

    regFormat.setForeground(QColor(236, 100, 75));

    for (const QString &pattern : Core()->regs) {
        rule.pattern.setPattern("\\b" + pattern + "\\b");
        rule.pattern.setPatternOptions(QRegularExpression::CaseInsensitiveOption);
        rule.format = regFormat;
        highlightingRules.append(rule);
    }

    singleLineCommentFormat.setFontWeight(QFont::Bold);
    singleLineCommentFormat.setForeground(QColor(63, 195, 128));
    rule.pattern.setPattern(";[^\n]*");
    rule.format = singleLineCommentFormat;
    highlightingRules.append(rule);

    commentStartRegularExpression.setPattern("/\\*");
    commentEndRegularExpression.setPattern("\\*/");
}

void Highlighter::highlightBlock(const QString &text)
{
    for (const HighlightingRule &rule : highlightingRules) {
        const QRegularExpression expression(rule.pattern);
        int index = expression.match(text).capturedStart();
        while (index >= 0) {
            const int length = expression.match(text).capturedLength();
            setFormat(index, length, rule.format);
            index = expression.match(text.mid(index + length)).capturedStart();
        }
    }
    setCurrentBlockState(0);

    int startIndex = 0;
    if (previousBlockState() != 1)
        startIndex = QRegularExpression(commentStartRegularExpression).match(text).capturedStart();

    while (startIndex >= 0) {
        const QRegularExpressionMatch commentEndMatch =
                QRegularExpression(commentEndRegularExpression).match(text.mid(startIndex));
        const int endIndex = commentEndMatch.capturedStart();
        int commentLength;
        if (endIndex == -1) {
            setCurrentBlockState(1);
            commentLength = text.length() - startIndex;
        } else {
            commentLength = endIndex - startIndex + commentEndMatch.capturedLength();
        }
        setFormat(startIndex, commentLength, multiLineCommentFormat);
        startIndex = QRegularExpression(commentStartRegularExpression)
                             .match(text.mid(startIndex + commentLength))
                             .capturedStart();
    }
}
