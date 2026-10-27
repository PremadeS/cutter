# Cutter Test Suite Plan

Oct 25, 2026 · @PremadeS

## Goal and rules

The suite guards what Cutter itself does as a frontend to rizin, so the `CutterCore` split and later refactors can't break it silently. Cutter owns four things, and only these are tested:

1. **Translation**: rizin data becomes models, columns, text and filters.
2. **Notification**: a state change emits the right signal, and views refresh.
3. **Action wiring**: a menu item, dialog or shortcut ends up as the right core call.
4. **Cutter-owned state**: seek history, IO modes, config, layouts, tasks.

Rules for every test:

- Never assert rizin's results (function counts, names, analysis output).
- Name the Cutter bug the test would catch. If you can't, don't write it.
- Every fixed bug gets a regression test at the lowest layer that reproduces it.
- No silent passes: a missing precondition is a `QSKIP` or a failure, never a bare `return`.
- Tests reach the core only through `common/TestEnv`, so the refactor changes one file, not every test.
- A test asserts the correct behavior even when Cutter currently gets it wrong. It stays red until the bug is fixed; tests must clean up so one failure never causes another.

## Directory split

Each directory is one layer and one ctest label, so the failing layer tells you which side of the core/widget boundary broke.

| Directory | Needs | Contains | A failure means |
| --- | --- | --- | --- |
| `unit/` | Nothing | Cutter's own algorithms and pure classes | Cutter logic regressed |
| `session/` | Core + fixture, no widgets | Core operation emits the right signal, once, with the right data | Core plumbing broke |
| `dialogs/` | App, or core + fixture | One dialog's flow: cancel, re-enter, error states, what its input does | That dialog's flow broke |
| `workflows/` | Full MainWindow + fixture | User scenarios across several widgets | Widget wiring broke |
| `common/` | — | `TestEnv`, `Fixtures.h`, `UiHelpers.h`; no tests | — |

`session/` maps to `RizinWrapper` + `CoreSession` after the split. `workflows/` files are named after a behavior, not a widget, so they survive widget rewrites and reuse in the diff window.

## common/

| File | Covers |
| --- | --- |
| `TestEnv.h` / `TestEnv.cpp` | Creates the core or the app, loads and analyzes fixtures, isolates settings, provides each test's `main`. The only code that knows how the core is created |
| `Fixtures.h` | Named paths into rizin-testbins, so tests never hardcode binaries |
| `UiHelpers.h` | Finding visible dialogs and closing message boxes while recording their text |

## unit/

| File | Status | Covers |
| --- | --- | --- |
| `test_AbstractFilterView` | Exists, extended | Quick-filter debounce, clear, close and show; rapid typing gives one filter update, not one per keystroke |
| `test_AddressableFilterProxy` | Written | Address and name stay correct through a sorted, filtered proxy, so activating a row seeks to the right place |
| `test_BasicInstructionHighlighter` | Exists | Per-instruction highlight ranges, including partial and full clears |
| `test_BinaryTrees` | Exists | Cutter's min/max trees used by the graph layout |
| `test_DisassemblyHelper` | Exists | Bracket ranges and reading address/context from a disassembly text cursor |
| `test_GraphLayouts` | Exists | Graph layout algorithms: empty, linear, branching, cyclic, horizontal and Graphviz |
| `test_Helpers` | Exists | `qhelpers` utilities: byte-count formatting, signal-free checks, combo selection, SVG recolouring, tree rows |
| `test_Json` | Exists | Rendering JSON values into tree widget items |
| `test_LinkedListPool` | Exists | The pooled linked list used by the graph layout |
| `test_MdHighlighter` | Exists | Markdown highlighting |
| `test_PjHandle` | Exists | Ownership of rizin's JSON builder: lifetime and moves |
| `test_RichTextPainter` | Exists | Rich text to HTML, and cropping with truncation indicators |
| `test_SelectionHighlight` | Exists | Same-word and brace-matching highlights |
| `test_ShortcutManager` | Written | No two default shortcuts share a key in the same context, every shortcut has a key on the current platform, and key matching checks all of a shortcut's keys |
| `test_StringsFilter` | Exists | Strings section + wildcard filter semantics; regression guard for the section filter bug |
| `test_SyntaxHighlighter` | Exists | The fallback syntax highlighter: keywords, comments, strings |

## session/

A session test belongs here only when the signal choice is non-trivial, or a regression would be hard to trace from a workflow failure.

| File | Status | Covers |
| --- | --- | --- |
| `test_AsyncTasks` | Written | Tasks report finished exactly once and leave the manager; an interrupted analysis finishes quickly (core side of bug 1) |
| `test_CoreLocking` | Written | Slots can read the core during its signals, and a task started from a slot runs once the signal returns. Guards the lock move in the CoreSession split |
| `test_CoreSignals` | Written | The signal contract of core edits: comments, function and variable renames, flags, byte edits, undefining functions. Each emits the right signal once, with the right data. Fails today on `renameFunction` with no function at the address |
| `test_IOModes` | Written | Cache mode never touches the file, committing the cache and write mode do, and cache and write mode stay mutually exclusive across switches. Fails today on commit leaving write mode on |
| `test_SeekHistory` | Written | Seeks notify with the right address and history type; same-address, invalid and silent seeks don't notify |
| `test_SessionLifecycle` | Planned | Tearing down a core while a task runs. Needs a fix first: `~CutterCore` frees rizin before task threads stop |

## dialogs/

One dialog's behavior per file, reached the way Cutter reaches it. Flow dialogs run in the real app before any docks are shown; single dialogs run on a core with a loaded file. Assertions are about what the dialog does, never about what a view shows.

A dialog gets tests when it turns input into different actions, validates or transforms input, owns Cutter state or a flow, or has a known bug. Form-only dialogs, dialogs that only display rizin data, and debug dialogs don't.

| File | Status | Covers |
| --- | --- | --- |
| `test_AsyncTaskDialog` | Written | Cancelling analysis from the progress dialog stops it quickly and leaves a usable window (bug 1) |
| `test_CommentsDialog` | Written | Add, edit (prefilled), clear-to-delete and cancel through the comment dialog |
| `test_EditMethodDialog` | Written | OK needs both names, auto-rename follows the real name only when enabled, new methods land in the class, editing renames the method |
| `test_EditVariablesDialog` | Written | Renames apply to the dialog's function and are sanitized, also when the seek is in another function. Likely fails today: the edit is looked up at the current seek |
| `test_FlagDialog` | Written | The dialog's add / rename / delete / no-op decisions, with prefill of an existing flag |
| `test_GlobalVariableDialog` | Written | The same add / modify / delete / no-op table for global variables, with prefill of name and type |
| `test_HexdumpRangeDialog` | Written | End and length modes stay in sync, the end becomes an inclusive last address, invalid, empty and overflowing ranges disable OK |
| `test_InitialOptionsDialog` | Written | Cancel returns to file selection without starting anything; architecture, bits, endianness and analysis level reach the options analysis runs with |
| `test_LayoutManager` | Written | Built-in layouts are hidden, rename refuses taken and reserved names and keeps the layout, delete needs confirmation |
| `test_MarkDialog` | Written | New marks get a proposed name, an inverted range or empty name is rejected, rename replaces the mark and keeps range and comment |
| `test_NewFileDialog` | Written | A second file after cancelling InitialOptions is accepted (bug 3); an error for a missing file doesn't come back once a valid file is chosen (bug 4) |

Not tested on their own: form-only dialogs (`EditFunctionDialog`, `EditStringDialog`, `SetToDataDialog`, `IntervalDialog`, `WriteCommandsDialogs`), display-only dialogs (`AboutDialog`, `VersionInfoDialog`, `RizinPluginsDialog`, `RizinTaskDialog`, `TypesVariablesDialog`, `StringListDialog`), rizin-driven ones (`EditInstructionDialog`, `TypesInteractionDialog`), `XrefsDialog` (covered in `workflows/`), and the debug dialogs.

## workflows/

These are slow and layout-sensitive, so each one is a scenario a user would notice breaking.

| File | Status | Covers |
| --- | --- | --- |
| `test_Docking` | Moved from `integration/` | Cutter's dock behavior: modifier-blocked docking, the visibility signal, closing transient docks |
| `test_EditPropagation` | Planned | A rename, comment or decompiler edit made in one view shows up in every other view |
| `test_ListFilters` | Planned | The strings section filter end to end, and filters staying applied after a refresh |
| `test_Navigation` | Written, extend | Activating a function moves disassembly and graph. To add: every addressable dock seeks on activation, back/forward, jumping from xrefs |
| `test_SessionWiring` | Planned | Every dock uses its MainWindow's session. Added after refactor step 2 |
| `test_Shortcuts` | Planned | Hexdump copy works (bug 5), Ctrl+F focuses a list's filter, keys act only on the focused widget |
| `test_ViewState` | Planned | Scroll and cursor survive an edit in decompiler (bug 2), disassembly and graph |

## Fixtures, CI and order of work

- **Fixtures** come from rizin-testbins via `-DCUTTER_TESTBINS_DIR`. Without it, `session/` and `workflows/` report as skipped. Named paths live in `common/Fixtures.h`; keep the set small.
- **CI** clones rizin-testbins at a pinned commit and runs every layer on Linux, macOS and Windows. A test that is flaky on one OS gets a `QSKIP` on that OS with a reason, not a whole layer disabled.
- **clang-tidy** currently skips `test/`. Add `test/.clang-tidy` (inherit the parent config, ignore `_data$` names) and extend the script's regex, then drop the `NOLINT`s.

Order of work:

1. Before refactor step 2, these must be green: `test_CoreSignals`, `test_NewFileDialog`, `test_Navigation`, `test_EditPropagation`.
2. The remaining tests from the bug list: `test_InitialOptionsDialog`, `test_ViewState`, `test_Shortcuts`.
3. Everything else, then `test_SessionWiring` once widgets take a session pointer.
4. Every new bug: a test in the matching file, or a new row in this doc if it opens a new bug class.
