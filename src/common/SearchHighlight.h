#ifndef SEARCHHIGHLIGHT_H
#define SEARCHHIGHLIGHT_H

class QPlainTextEdit;
class QString;

// TODO: DOCSSSSSSSSSS
// Will contain all search highlight related helpers
namespace SearchHighlight {

int updateSearchHighlights(QPlainTextEdit *textEdit, const QString &word, int mode);

// TODO: better name for bool setF..
void moveFocus(QPlainTextEdit *textEdit, int toIndex, int fromIndex = -1, bool setFromBg = true,
               bool setCursor = true);
};

#endif // SEARCHHIGHLIGHT_H
