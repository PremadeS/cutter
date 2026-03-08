#ifndef SEARCHHIGHLIGHT_H
#define SEARCHHIGHLIGHT_H

#include <QPair>

class QPlainTextEdit;
class QString;

// Probably will move to a subclass of plaintextedit
// TODO: DOCSSSSSSSSSS
// Will contain all search highlight related helpers
namespace SearchHighlight {

int updateSearchHighlights(QPlainTextEdit *textEdit, const QString &word, int mode);

// TODO: better name for bool setF..
void moveFocus(QPlainTextEdit *textEdit, int toIndex, int fromIndex = -1, bool setFromBg = true,
               bool setCursor = true);

// returns number of matches
// maybe QPair if index and count?
int search(QPlainTextEdit *textEdit, const QString &str, int &index);

void highlightSearch(QPlainTextEdit *textEdit, const QList<int> &positions, const int strLen,
                     int &index);
};

#endif // SEARCHHIGHLIGHT_H
