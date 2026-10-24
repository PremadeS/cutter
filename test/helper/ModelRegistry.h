#ifndef MODEL_REGISTRY_H
#define MODEL_REGISTRY_H

/*
 * Generic registry of Cutter list/table models and their proxy models.
 *
 * The registry powers the L1 "model conformance" batteries in
 * test/unit/tst_ModelConformance.cpp. The batteries themselves are generic:
 * they iterate every entry below and apply the same structural assertions.
 * Adding a widget's model to the suite is a matter of adding one entry here —
 * no new test executable and no per-widget test code.
 *
 * Design notes:
 *  - `make` constructs the source model AND its proxy. The proxies take their
 *    concrete source type, so each entry knows how to wire the pair.
 *  - `coreDependentColumns` lists columns whose data()/lessThan() call into
 *    Core() (overwhelmingly the "Comment" column, which reads
 *    Core()->getCommentAt()). The batteries SKIP these columns so the L1 tests
 *    stay free of Core()/rizin. Over-listing a column is safe (less coverage),
 *    under-listing would crash.
 *  - `addressable` marks models implementing AddressableItemModelI (whose
 *    address()/name() the battery exercises).
 *  - `populate`, when set, injects synthetic non-empty data into the source
 *    model so the batteries can run their populated-mode checks (row data,
 *    filtering, sorting). Only models that expose a test seam get one; the
 *    flagship is StringsModel::setStrings(). Entries without `populate` only
 *    run the empty-mode structural checks.
 */

#include "common/AddressableItemModel.h"

#include "widgets/StringsWidget.h"
#include "widgets/SectionsWidget.h"
#include "widgets/FlagsWidget.h"
#include "widgets/ImportsWidget.h"
#include "widgets/ExportsWidget.h"
#include "widgets/SymbolsWidget.h"
#include "widgets/HeadersWidget.h"
#include "widgets/RelocsWidget.h"
#include "widgets/MemoryMapWidget.h"
#include "widgets/SegmentsWidget.h"
#include "widgets/SearchWidget.h"
#include "widgets/GlobalsWidget.h"
#include "widgets/TypesWidget.h"
#include "widgets/BreakpointWidget.h"
#include "widgets/RegisterRefsWidget.h"
#include "widgets/FlirtWidget.h"

#include <QAbstractItemModel>
#include <QSet>
#include <QSortFilterProxyModel>
#include <QString>
#include <QVariant>

#include <functional>
#include <utility>

struct ModelUnderTest {
    QAbstractItemModel *source = nullptr;
    QSortFilterProxyModel *proxy = nullptr;
};

struct ModelEntry {
    QString name;
    std::function<ModelUnderTest()> make;
    QSet<int> coreDependentColumns;
    bool addressable = false;
    // Empty std::function means "no populate hook" (empty-mode checks only).
    std::function<void(QAbstractItemModel *)> populate;
};

// Helper to build a synthetic StringDescription.
inline StringDescription makeTestString(RVA vaddr, const QString &string, const QString &type,
                                        const QString &section, quint32 length, quint32 size)
{
    StringDescription s;
    s.vaddr = vaddr;
    s.string = string;
    s.type = type;
    s.section = section;
    s.length = length;
    s.size = size;
    return s;
}

inline QList<ModelEntry> allModelEntries()
{
    QList<ModelEntry> entries;

    // --- Strings ---
    {
        ModelEntry e;
        e.name = QStringLiteral("Strings");
        e.make = []() {
            auto *src = new StringsModel();
            auto *proxy = new StringsProxyModel(src);
            return ModelUnderTest{ src, proxy };
        };
        // StringsModel::data() and StringsProxyModel::lessThan() call
        // Core()->getCommentAt() for the Comment column.
        e.coreDependentColumns = { StringsModel::CommentColumn };
        e.addressable = true;
        e.populate = [](QAbstractItemModel *m) {
            auto *sm = static_cast<StringsModel *>(m);
            QList<StringDescription> strings;
            strings << makeTestString(0x1000, "hello world", "latin1", ".text", 11, 11);
            strings << makeTestString(0x1010, "cutter rocks", "utf8", ".rodata", 12, 12);
            strings << makeTestString(0x1020, "another string", "latin1", ".text", 14, 14);
            strings << makeTestString(0x1030, "final entry", "utf8", ".data", 11, 11);
            sm->setStrings(strings);
        };
        entries << e;
    }

    // --- Sections ---
    {
        ModelEntry e;
        e.name = QStringLiteral("Sections");
        e.make = []() {
            auto *src = new SectionsModel();
            auto *proxy = new SectionsProxyModel(src);
            return ModelUnderTest{ src, proxy };
        };
        e.coreDependentColumns = { SectionsModel::CommentColumn };
        e.addressable = true;
        entries << e;
    }

    // --- Flags ---
    {
        ModelEntry e;
        e.name = QStringLiteral("Flags");
        e.make = []() {
            auto *src = new FlagsModel();
            auto *proxy = new FlagsSortFilterProxyModel(src);
            return ModelUnderTest{ src, proxy };
        };
        e.coreDependentColumns = { FlagsModel::COMMENT };
        e.addressable = true;
        entries << e;
    }

    // --- Imports ---
    {
        ModelEntry e;
        e.name = QStringLiteral("Imports");
        e.make = []() {
            auto *src = new ImportsModel();
            auto *proxy = new ImportsProxyModel(src);
            return ModelUnderTest{ src, proxy };
        };
        e.coreDependentColumns = { ImportsModel::CommentColumn };
        e.addressable = true;
        entries << e;
    }

    // --- Exports ---
    {
        ModelEntry e;
        e.name = QStringLiteral("Exports");
        e.make = []() {
            auto *src = new ExportsModel();
            auto *proxy = new ExportsProxyModel(src);
            return ModelUnderTest{ src, proxy };
        };
        e.coreDependentColumns = { ExportsModel::CommentColumn };
        e.addressable = true;
        entries << e;
    }

    // --- Symbols ---
    {
        ModelEntry e;
        e.name = QStringLiteral("Symbols");
        e.make = []() {
            auto *src = new SymbolsModel();
            auto *proxy = new SymbolsProxyModel(src);
            return ModelUnderTest{ src, proxy };
        };
        e.coreDependentColumns = { SymbolsModel::CommentColumn };
        e.addressable = true;
        entries << e;
    }

    // --- Headers ---
    {
        ModelEntry e;
        e.name = QStringLiteral("Headers");
        e.make = []() {
            auto *src = new HeadersModel();
            auto *proxy = new HeadersProxyModel(src);
            return ModelUnderTest{ src, proxy };
        };
        e.coreDependentColumns = { HeadersModel::CommentColumn };
        e.addressable = true;
        entries << e;
    }

    // --- Relocs ---
    {
        ModelEntry e;
        e.name = QStringLiteral("Relocs");
        e.make = []() {
            auto *src = new RelocsModel();
            auto *proxy = new RelocsProxyModel(src);
            return ModelUnderTest{ src, proxy };
        };
        e.coreDependentColumns = { RelocsModel::CommentColumn };
        e.addressable = true;
        entries << e;
    }

    // --- MemoryMap ---
    {
        ModelEntry e;
        e.name = QStringLiteral("MemoryMap");
        e.make = []() {
            auto *src = new MemoryMapModel();
            auto *proxy = new MemoryProxyModel(src);
            return ModelUnderTest{ src, proxy };
        };
        e.coreDependentColumns = { MemoryMapModel::CommentColumn };
        e.addressable = true;
        entries << e;
    }

    // --- Segments ---
    {
        ModelEntry e;
        e.name = QStringLiteral("Segments");
        e.make = []() {
            auto *src = new SegmentsModel();
            auto *proxy = new SegmentsProxyModel(src);
            return ModelUnderTest{ src, proxy };
        };
        e.coreDependentColumns = { SegmentsModel::CommentColumn };
        e.addressable = true;
        entries << e;
    }

    // --- Search ---
    {
        ModelEntry e;
        e.name = QStringLiteral("Search");
        e.make = []() {
            auto *src = new SearchModel();
            auto *proxy = new SearchSortFilterProxyModel(src);
            return ModelUnderTest{ src, proxy };
        };
        e.coreDependentColumns = { SearchModel::COMMENT };
        e.addressable = true;
        entries << e;
    }

    // --- Globals ---
    {
        ModelEntry e;
        e.name = QStringLiteral("Globals");
        e.make = []() {
            auto *src = new GlobalsModel();
            auto *proxy = new GlobalsProxyModel(src);
            return ModelUnderTest{ src, proxy };
        };
        e.coreDependentColumns = { GlobalsModel::CommentColumn };
        e.addressable = true;
        entries << e;
    }

    // --- Types (plain QAbstractListModel, not AddressableItemModel) ---
    {
        ModelEntry e;
        e.name = QStringLiteral("Types");
        e.make = []() {
            auto *src = new TypesModel();
            auto *proxy = new TypesSortFilterProxyModel(src);
            return ModelUnderTest{ src, proxy };
        };
        // TypesModel has no Comment column.
        e.coreDependentColumns = {};
        e.addressable = false;
        entries << e;
    }

    // --- Breakpoint ---
    {
        ModelEntry e;
        e.name = QStringLiteral("Breakpoint");
        e.make = []() {
            auto *src = new BreakpointModel();
            auto *proxy = new BreakpointProxyModel(src);
            return ModelUnderTest{ src, proxy };
        };
        e.coreDependentColumns = { BreakpointModel::CommentColumn };
        e.addressable = true;
        entries << e;
    }

    // --- RegisterRef (plain QAbstractListModel) ---
    {
        ModelEntry e;
        e.name = QStringLiteral("RegisterRef");
        e.make = []() {
            auto *src = new RegisterRefModel();
            auto *proxy = new RegisterRefProxyModel(src);
            return ModelUnderTest{ src, proxy };
        };
        e.coreDependentColumns = { RegisterRefModel::CommentColumn };
        e.addressable = false;
        entries << e;
    }

    // --- Flirt (plain QAbstractListModel) ---
    {
        ModelEntry e;
        e.name = QStringLiteral("Flirt");
        e.make = []() {
            auto *src = new FlirtModel();
            auto *proxy = new FlirtProxyModel(src);
            return ModelUnderTest{ src, proxy };
        };
        // FlirtModel has no Comment column.
        e.coreDependentColumns = {};
        e.addressable = false;
        entries << e;
    }

    return entries;
}

#endif // MODEL_REGISTRY_H
