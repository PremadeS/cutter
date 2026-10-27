# CLAUDE.md

Context for agents working in this repo. Read this instead of re-exploring `src/`.

Cutter is a Qt 6 / C++20 GUI frontend for the rizin reverse-engineering framework.
`rizin/` is a git submodule. All app code is in `src/` and is built as a static
library `cutter_gui_lib` (linked by both the `Cutter` executable and the tests).

## Ongoing refactor

`CutterCore` is being split (PR rizinorg/cutter#3654) into:

- **RizinWrapper**: owns one `RzCore`, converts rizin C structs into Cutter
  descriptors, emits signals.
- **CoreSession**: GUI coordination (`trigger*()` methods, request signals),
  owns `AsyncTaskManager`, the wrapper and its lock.
- **DynamicSession**: CoreSession + debugging.

Step 2 removes the singleton and passes a session pointer to each widget
(all widgets get the same pointer for now). Until then, everything below still
says `CutterCore` / `Core()`.

## Global singletons (macros)

| Macro | Class | Defined in |
| --- | --- | --- |
| `Core()` | `CutterCore` | `core/Cutter.h` |
| `Config()` | `Configuration` (QSettings-backed prefs, colors, fonts, recent files) | `common/Configuration.h` |
| `Shortcuts()` | `ShortcutManager` | `shortcuts/ShortcutManager.h` |
| `Plugins()` | `PluginManager` | `plugins/PluginManager.h` |
| `Python()` | `PythonManager` (only with `CUTTER_ENABLE_PYTHON`) | `common/PythonManager.h` |

## src/ layout

| Path | Contents |
| --- | --- |
| `Main.cpp` | Sets org/app name, settings migration, creates `CutterApplication` |
| `CutterApplication.*` | Parses CLI options, initializes core, config, plugins, decompilers; creates `MainWindow`; opens a file or shows `NewFileDialog` |
| `core/Cutter.*` | `CutterCore`: ~1100-line header, ~5500-line cpp. Every rizin call, all data getters, edits, seek, config, signals |
| `core/CutterDescriptions.h` | Plain structs Cutter uses for rizin data (`FunctionDescription`, `StringDescription`, `FlagDescription`, `VariableDescription`, `DisassemblyLine`, ...) |
| `core/CutterCommon.h` | `RVA`, `RVA_INVALID`, common macros |
| `core/RizinCpp.*` | C++ helpers over rizin C types: `CutterRzList`, `CutterPVector`, `CutterRzIter`, `CutterRzListForeach` |
| `core/MainWindow.*` | Owns every dock, menus, layouts, file open flow (`openNewFile`, `displayInitialOptionsDialog`, `finalizeOpen`), project save/load |
| `core/CutterJson.*`, `core/Basefind.*` | JSON wrapper over rizin JSON; base-address finder |
| `common/` | Non-widget logic: tasks, config, highlighters, helpers, seekable, refresh deferral, decompiler interface, Python |
| `widgets/` | All dock widgets, item models, graph views, filter views (~60 classes) |
| `dialogs/` | All dialogs; `dialogs/preferences/` holds the Preferences pages |
| `menus/` | Context menus: addressable item, disassembly, decompiler, FLIRT |
| `shortcuts/` | `ShortcutManager` + `DefaultShortcuts` table |
| `plugins/` | `CutterPlugin` interface, `PluginManager`, C++ and Python samples |
| `python/`, `bindings/` | Embedded Python and the Shiboken bindings |
| `tools/basefind/` | Base-find dialog |
| `themes/`, `img/`, `fonts/`, `translations/`, `resources.qrc` | Assets |

## Key mechanics

**Locking.** Inside `CutterCore`, `CORE_LOCK()` (`RzCoreLocked core(this)`)
takes the recursive mutex for the method's scope. Outside it, use
`RzCoreLocked core(Core())` or `Core()->lock()`. Signals are emitted while the
lock is held, so slots run under it and may call back into the core.

**Typical core method.** Lock, call rizin, `emit`. Edit → signal:

| Operation | Signal |
| --- | --- |
| `setComment`, `delComment` | `commentsChanged(addr)` |
| `renameFunction` | `functionRenamed(offset, name)` |
| `delFunction` | `functionsChanged` |
| `renameFunctionVariable` | `refreshCodeViews` |
| `addFlag`, `renameFlag`, `delFlag` | `flagsChanged` (`renameFlag` returns early if the flag is missing) |
| `editBytes`, `editInstruction` | `instructionChanged(addr)` |
| global variables | `globalVarsChanged` |
| marks | `marksChanged` |

`setConfig` emits nothing; callers use `trigger*()` (`asmOptionsChanged`,
`graphOptionsChanged`, ...). The code views (disassembly, graph, decompiler)
refresh on both `refreshCodeViews` and `varsChanged`.

**Seeking.** `seek(ut64)` emits `seekChanged(addr, SeekHistoryType)` only when the
offset actually changes, and ignores `RVA_INVALID`. `seekPrev`/`seekNext` emit
`Undo`/`Redo`. `seekSilent` moves without emitting. `seekAndShow` also emits
`showAddressRequested`. Each addressable dock has a `CutterSeekable`
(`common/CutterSeekable.h`); when synchronized, its `getOffset()` simply returns
`Core()->getOffset()`, so it says nothing about what the widget displays.

**IO modes.** `setIOCache` and `setWriteMode` keep cache and write mode mutually
exclusive by calling each other; one switch can emit `ioModeChanged` twice.
Write mode reopens the file RW, read-only reopens it RO. `commitWriteCache`
temporarily reopens RW to flush the cache.

**Refresh deferral.** Hidden docks don't refresh. `RefreshDeferrer`
(`common/RefreshDeferrer.h`) marks them dirty and refreshes when
`CutterDockWidget::becameVisibleToUser` fires.

**Async work.** `AsyncTask` (`common/AsyncTask.h`) runs on `AsyncTaskManager`'s
thread pool (`Core()->getAsyncTaskManager()`). `start()` emits `tasksChanged`, the
task's `finished` is handled on the manager's thread (removal, second
`tasksChanged`). `AsyncTask::wait()` returns immediately if the task hasn't begun
running yet. File loading and analysis is `AnalysisTask` (`setOptions`,
`interrupt` → rizin cons break); `AsyncTaskDialog` shows progress and interrupts
on Cancel or close. `RizinTask` wraps rizin's own task system.

**Decompilers.** `common/Decompiler.h` is the interface;
`Core()->registerDecompiler()` adds one (rz-ghidra if built in, or from plugins).

**Settings.** `Main.cpp` sets the org/app name and `QSettings::IniFormat`
(via `Cutter::initializeSettings`). `Config()->isFirstExecution()` makes
`CutterApplication` show the modal `WelcomeDialog`. Useful CLI flags:
`-A <level>` (skip InitialOptions), `--no-output-redirect`,
`--no-cutter-plugins`, `--no-rizin-plugins`.

**Resources.** `cutter_gui_lib` is static with `resources.qrc` compiled in, so
other executables linking it call `Q_INIT_RESOURCE(resources)`.

## Widget hierarchy

```
QDockWidget
└── CutterDockWidget            visibility tracking, transient docks
    ├── ListDockWidget          tree view + QuickFilterView (most list docks)
    ├── SearchableDockWidget
    └── AddressableDockWidget   owns a CutterSeekable
        └── MemoryDockWidget    DisassemblyWidget, GraphWidget, HexdumpWidget, DecompilerWidget
```

List docks pair a model and a proxy: `XModel : AddressableItemModel<...>` and
`XProxyModel : AddressableFilterProxyModel` (`common/AddressableItemModel.h`).
`AddressableItemModelI::address(index)` gives the row's address.
`AddressableItemListImpl<T>` (`widgets/AddressableItemList.h`, used as
`AddressableTreeView` in list docks) seeks on `activated` and shows
`AddressableItemContextMenu`. Example: `widgets/FunctionsWidget.*`,
`widgets/StringsWidget.*`.

Observable widget state: `DisassemblyWidget::getLines()` (each `DisassemblyLine`
has `offset`); `GraphWidget::getGraphView()->getBlocks()` (keyed by block entry).

Graph code: `GraphView` (base) → `CutterGraphView` → `DisassemblerGraphView`,
with layouts in `GraphGridLayout`, `GraphvizLayout`, `GraphHorizontalAdapter`.

## File open flow

1. `NewFileDialog::loadFile()` checks the file with `Core()->tryFile()`. On
   failure it shows a modal `QMessageBox` and stays open. On success it prepends
   to `Config()` recent files and calls `MainWindow::openNewFile()` with
   `filename = ioPlugin text ("file://" by default) + path`.
2. `MainWindow::openNewFile()` shows `InitialOptionsDialog` (or skips it with
   `-A`).
3. InitialOptions' Cancel → `reject()` → `MainWindow::displayNewFileDialog()`
   (a new NewFileDialog).
4. OK → `setupAndStartAnalysis()` builds `InitialOptions` from the UI (arch/bits
   combos, endianness index 0/1/2 = Auto/Little/Big, slider 0..3 = none / `aaa` /
   `aaaa` / advanced checkboxes). It starts an `AnalysisTask`, shows an
   `AsyncTaskDialog`, and stores the options with
   `CutterApplication::setInitialOptions()`.
5. On `finished`: `openFileFailed` → `MainWindow::openNewFileFailed()` (modal
   box); otherwise `MainWindow::finalizeOpen()` refreshes and shows the window.

One file per process: after `finalizeOpen()` the open dialogs are not reached
again, and File → New (`onActionNewTriggered`) starts a new Cutter process.

## Dialog patterns

- **Decision dialogs** turn one form into several actions depending on existing
  state. `FlagDialog` and `GlobalVariableDialog` map name empty/non-empty ×
  exists/doesn't to delete / rename / add / no-op. `CommentsDialog::addOrEditComment`
  prefills, deletes on empty, sets otherwise.
- **Validating dialogs** check input before acting: `MarkDialog` (hex, start ≤
  end, non-empty name; rename = delete + add), `HexdumpRangeDialog` (end/length
  modes, recalculated only on `textEdited`), `LayoutManager` (no empty, built-in
  or taken names).
- **Static exec helpers** run a modal dialog and apply the result:
  `CommentsDialog::addOrEditComment`, `EditMethodDialog::newMethod/editMethod`,
  the `QInputDialog`/`QMessageBox` statics in `LayoutManager`.
- **Form-only dialogs** (`EditFunctionDialog`, `EditStringDialog`,
  `SetToDataDialog`, `IntervalDialog`, `WriteCommandsDialogs`) have only setters
  and getters; the caller applies the result.

## Known issues found by the tests

Not fixed yet; tests assert the correct behavior and fail until they are.

- `CutterCore::renameFunction` ignores rizin's result and emits `functionRenamed`
  even when no function exists at the address.
- `CutterCore::commitWriteCache` reads `desc->fd` after the reopen replaced the
  descriptor, so it never returns to read-only and stays in write mode.
- `EditVariablesDialog::applyFields` finds the variable in the function at the
  current seek, not the dialog's function; the edit is lost when they differ.
- `InitialOptions::endian` has no default initializer.
- `~CutterCore` frees `RzCore` before the task thread pool stops (likely
  use-after-free if a task is running).

## Conventions

- Format with `_clang-format`. clang-tidy (`.clang-tidy`, run via
  `scripts/clang-tidy.py`) only checks `src/`; warnings are errors.
- Identifiers: functions, methods and variables `camelBack`, classes `CamelCase`.
- Exported classes use `CUTTER_EXPORT`.

## Branches, build and packaging

**Branches.** Cutter, rizin and the rizinorg plugins all use `dev` + `stable`.
Cutter `dev` pins the rizin submodule to a rizin `dev` commit; Cutter `stable`
pins a rizin release tag (v0.9.1 as of Oct 2026) and is where release tags
(`v*`, e.g. `v2.5.0-rc4`) are cut. A bundled plugin must match the rizin it is
built against, so each Cutter branch hardcodes its plugin refs:

| Plugin | How it is fetched | Ref on Cutter `dev` | Ref on Cutter `stable` |
| --- | --- | --- | --- |
| rz-ghidra | `ExternalProject_Add` in `dist/CMakeLists.txt` | `GIT_TAG dev` | `GIT_TAG v0.9.0` |
| jsdec | `scripts/jsdec.sh`, `dist/bundle_jsdec.ps1` | `--branch dev` | `--branch v0.9.0` |
| rz-libyara | `scripts/rz-libyara.sh`, `dist/bundle_rz_libyara.ps1` | `--branch main` | same |
| rz-silhouette, rz-libswift | `scripts/rz-*.sh`, `dist/bundle_rz_*.ps1` | default branch | default branch |
| rz-frida | `scripts/rz-frida.sh` (Linux/macOS), `dist/bundle_rz_frida.ps1` (Windows) | default branch (now `dev`) | not bundled yet |

rz-frida has `dev` (default) and `stable` branches and one tag (`v0.1.0`).
Its scripts also pin `FRIDA_VERSION` (frida-core devkit, currently 17.17.0),
duplicated in the `.sh`, the `.ps1` and rz-frida's own `meson_options.txt`.

**Plugin scripts.** Each takes `<install-prefix> [extra-cmake-opts]`, clones
with `--depth 1` only if the checkout is missing (a stale local checkout is
reused, even across branch switches), builds the rizin plugin with meson into
the prefix, then the Cutter plugin with CMake. They run from `install(CODE ...)`
in `dist/CMakeLists.txt` when `CUTTER_PACKAGE_<NAME>=ON`; on Windows also only
with `CUTTER_ENABLE_DEPENDENCY_DOWNLOADS=ON`. Windows and macOS install Cutter
plugins to `plugins/native`.

**Key CMake options.** `CUTTER_USE_BUNDLED_RIZIN` (build the `rizin/` submodule,
`cmake/BundledRizin.cmake`), `CUTTER_ENABLE_PACKAGING`,
`CUTTER_ENABLE_DEPENDENCY_DOWNLOADS`, `CUTTER_ENABLE_PYTHON[_BINDINGS]`,
`CUTTER_ENABLE_GRAPHVIZ`, `CUTTER_ENABLE_SIGDB`, `CUTTER_APPIMAGE_BUILD`,
`CUTTER_QT` (5 or 6), `CUTTER_PACKAGE_{RZ_GHIDRA,JSDEC,RZ_LIBSWIFT,RZ_LIBYARA,RZ_SILHOUETTE,RZ_FRIDA}`.
`scripts/fetch_deps.sh` downloads prebuilt Qt/Python/PySide into
`cutter-deps/` (source `cutter-deps/env.sh`).

**CI (`.github/workflows/ci.yml`).** Runs on push to `dev`/`stable`, PRs to
`dev`/`stable`, and `v*`/`upload-test*` tags; tag pushes upload release assets
(`softprops/action-gh-release`).
- `build-linux`: Ubuntu 22.04 containers: AppImage (`linux-x86_64`, all plugins
  packaged), `linux-x86_64-system-deps` (gcc-12, system Qt, no packaging),
  `tarball`.
- a second Linux job via `.github/actions/build-linux-old` (Qt5 AppImage and Qt5
  system-deps).
- `build`: `windows-x86_64` (windows-2022), `macos-x86_64` (macos-15-intel),
  `macos-arm64` (macos-14); all package with every plugin.
- `plugin-test`: extracts the built package and compiles
  `src/plugins/sample-cpp` against it on Linux, macOS and Windows.
- There is no `ctest` step yet; the test suite still has to be wired in.
- Other workflows: `linter.yml`, `coverity-scan.yml`, `docs.yml`.

## Tests

See `test/README.md` and `Cutter_Test_Suite_Plan.md`. Layers (one ctest label
each): `unit/` (no core), `session/` (core + fixture, no widgets), `dialogs/`
(one dialog's behavior), `workflows/` (full MainWindow). Rules: never assert
rizin's results; each test names the Cutter bug it would catch; tests assert
correct behavior even if Cutter is currently wrong; tests clean up so one failure
can't cause another.

`test/common/`:
- `TestEnv`: the only code that creates the core or app. It isolates settings
  (temp INI dir, `firstExecution=false`), defaults to offscreen, and initializes
  resources. It provides `CutterTest::core()`, `mainWindow()`, `openFile()`,
  `analyze()` and `addressWithoutComment()`. Each test's main is one of:
  - `CUTTER_CORE_TEST_MAIN` (QApplication + core)
  - `CUTTER_APP_TEST_MAIN` (CutterApplication showing NewFileDialog)
  - `CUTTER_APP_TEST_MAIN_WITH_FILE` (opens and analyzes a fixture with `-A 1`)
- `Fixtures.h`: testbins paths (`helloX64`, `crackmeX86`, `coreutilsLs`), resolved
  from `CUTTER_TESTBINS_DIR`. If a fixture is missing, the test skips (exit 77).
- `UiHelpers.h`: `findVisibleTopLevel<T>`, `setLineEditText`, `clickButton`,
  `clickStandardButton`, `onceVisible<T>` (act on a modal dialog), and
  `MessageBoxWatcher`.
- `CallbackTask.h`: an `AsyncTask` that runs a lambda.

Each `.cpp` is one QtTest executable; add it to its directory's
`CMakeLists.txt` (`cutter_add_layer_tests`). Files start with a short doxygen
`@file`/`@brief` header and have no other comments.
