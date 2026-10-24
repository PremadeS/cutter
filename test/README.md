# Cutter test suite

This directory holds Cutter's automated tests. They are organised into three
layers so that a failure points at the right culprit: **Cutter's own logic**,
**a change in the rizin underneath it**, or **widget wiring**.

The suite is built around *batteries*, not per-widget tests. A battery is one
generic loop that asserts a class of invariants over a **registry** of models,
Core getters, or discovered docks. Adding a widget means adding a registry
entry, not a new test file.

## Layers

### L1 — unit (`test/unit`, label `unit`)

Pure logic. **No `Core()`, no rizin, no display.** `CutterCore::instance()`
must stay `nullptr`; the tests assert this in `initTestCase()`.

| Battery | File | What it asserts |
| --- | --- | --- |
| A — model conformance | `tst_ModelConformance.cpp` | Every registered model obeys the `QAbstractItemModel` contract: sane `rowCount`/`columnCount`, valid headers, in-range indices, no crash on `data()`. Runs empty for all entries; populated for entries with a `populate` hook. Columns that call into `Core()` (e.g. the Comment column) are skipped via registry metadata. |
| B — proxy conformance | `tst_ModelConformance.cpp` | Registered proxies preserve row counts across filter/sort round-trips, honour wildcard + literal-metacharacter filters, and restore the exact count when a filter is cleared. |
| strings filter semantics | `tst_StringsFilter.cpp` | Direct regression guard for the reported bug: section + wildcard intersection, "(all)" clearing, empty sections, case-insensitivity, regex metacharacters treated literally. Driven by synthetic data via the `StringsModel::setStrings()` seam. |

Registries live in `test/helper/ModelRegistry.h`. To enrol a model, add one
`ModelEntry` (constructor factory, `coreDependentColumns`, `addressable`,
optional `populate` hook).

### L2 — Core contract (`test/integration`, label `integration`)

Core + rizin, **no GUI**. Runs against a real loaded binary
(`loadFile(path, 0, 0, RZ_PERM_R, 1, true)` + `aaa` — the `loadFile` defaults
do *not* load bin info, which would make everything vacuously empty).

| Battery | File | What it asserts |
| --- | --- | --- |
| C — API invariants | `tst_CoreContracts.cpp` | Every Core getter is deterministic, yields valid (non-`RVA_INVALID`) addresses, and the always-present getters are non-empty (anti-vacuously guarded). |
| D — cross-API consistency | `tst_CoreContracts.cpp` | The money rows. `getSectionList()` ↔ `getAllSections()` agree on the section-name set; **every `StringDescription.section` is a member of `getSectionList()`**; `getSectionAtAddress` resolves to a known section; functions and flags resolve by address. |
| E — edge states | `tst_CoreContracts.cpp` | Unload/reload empties and repopulates every bin-derived getter without crashing. |
| lifecycle | `tst_CoreLifeCycle.cpp` | Existing end-to-end Core lifecycle coverage (seek, config, flags, comments, editing, …). |

### L3 — widget integration (`test/integration`, label `integration`)

Core + rizin + GUI. Discovers every `CutterDockWidget` childed to `MainWindow`.
Runs headless via `QT_QPA_PLATFORM=offscreen`.

| Battery | File | What it asserts |
| --- | --- | --- |
| F — data presence | `tst_DockWidgets.cpp` | Whitelisted docks (see `dataPresenceDocks()`) are non-empty after `aaa`. New docks are enrolled by adding their class name to that set. |
| G — quick-filter round-trip | `tst_DockWidgets.cpp` | An unmatched token filters to **exactly zero** rows and clearing restores the **exact** original count (the old `rowCount <= initial` check could never catch a filter that "sticks"). |
| H — combo filter round-trip | `tst_DockWidgets.cpp` | `ComboQuickFilterView` entries never exceed the total and restore exactly. |
| I — column sorting | `tst_DockWidgets.cpp` | Sorting by every column preserves the row count. |
| strings section-combo wiring | `tst_DockWidgets.cpp` | End-to-end guard for the reported bug: the strings section combo exactly mirrors `getSectionList()`, and selecting a section surfaces exactly the strings whose `.section` equals it. |

All L3 waits use `QTRY_*`/`QSignalSpy` against real signals and the 150 ms
filter debounce — **never fixed sleeps**, so they cannot pass vacuously on slow
CI runners.

## Failure triage

| L1 | L2 | L3 | Meaning |
| --- | --- | --- | --- |
| red | — | — | **Cutter logic bug.** Model/proxy code regressed. |
| green | red | — | **rizin changed underneath.** A Core wrapper or the data it returns no longer matches Cutter's expectations — see the failing contract row in `tst_CoreContracts.cpp`. |
| green | green | red | **Widget wiring bug.** A dock's filter/sort/combo/refresh path is broken. |

Before fixing a widget for an apparent rizin break, add a failing contract row
to L2. That keeps the regression guarded at the cheapest layer that can catch
it.

## Running

```sh
# everything
ctest --test-dir build/<builddir>

# one layer
ctest --test-dir build/<builddir> -L unit
ctest --test-dir build/<builddir> -L integration

# one test
ctest --test-dir build/<builddir> -R unit.tst_ModelConformance
```

Integration tests set `QT_QPA_PLATFORM=offscreen` and a timeout automatically;
unit tests are pure logic and run without a display.
