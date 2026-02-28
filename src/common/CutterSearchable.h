#ifndef CUTTERSEARCHABLE_H
#define CUTTERSEARCHABLE_H

class QString;
class SearchBarWidget;
class QWidget;

enum SearchOption {
    CaseSensitive = 1,
    WholeWords = 1 << 1,
    RegExp = 1 << 2,
    HighlightMatches = 1 << 3
};

// TODO: DOC THIS THAT ITS INTERFACE BRUV
class CutterSearchable
{

public:
    virtual QWidget *searchableArea() const = 0;
    virtual int searchHPadding() const = 0;
    virtual int searchVPadding() const = 0;

    virtual void searchChanged(const QString &text, int options) = 0;
    virtual void findNext() = 0;
    virtual void findPrev() = 0;
    virtual void findLast() = 0;
    virtual void searchBarHidden() = 0;
    virtual void searchBarShown() = 0;
};

namespace CutterSearchableUtil {

void setupConnections(QWidget *parent, SearchBarWidget *bar);

void positionSearchBar(QWidget *parent, SearchBarWidget *bar, QWidget *area, int hPadding,
                       int vPadding);
};

#endif // CUTTERSEARCHABLE_H
