# Cutter test suite

Tests cover what Cutter itself does as a frontend to rizin: turning rizin data
into views, notifying views when state changes, wiring user actions to the core,
and the state Cutter owns. They do not test rizin's own results.

## Layout

| Directory | Needs | Tests |
| --- | --- | --- |
| `unit/` | nothing | Cutter's own algorithms and pure classes |
| `session/` | core + fixture, no widgets | core operations emit the right signal, once, with the right data |
| `dialogs/` | app, no loaded file | one dialog's flow: cancel, re-enter, error states |
| `workflows/` | full MainWindow + fixture | user scenarios across several widgets |
| `common/` | — | shared helper; the only code that knows how the core is created |

Each `ctest` label matches its directory.

## Fixtures

`session/` and `workflows/` load binaries from
[rizin-testbins](https://github.com/rizinorg/rizin-testbins), which is not part
of this repository:

```sh
cmake -B build -DCUTTER_TESTBINS_DIR=/path/to/rizin-testbins
```

Without it those tests are reported as skipped.

## Running

```sh
ctest --test-dir build                  # everything
ctest --test-dir build -L session       # one layer
ctest --test-dir build -R test_Navigation
```

Tests run headless (`QT_QPA_PLATFORM=offscreen`) with isolated settings, so they
never touch your own Cutter configuration.

## Adding a test

- Name it after the behavior or bug it guards (`test_<Area>.cpp`, one QtTest
  class per file).
- Add it to the `CMakeLists.txt` of its directory.
- If you can't name a Cutter bug it would catch, don't add it.
