# Cutter test suite

Tests cover what Cutter itself does as a frontend to rizin: turning rizin data
into views, notifying views when state changes, wiring user actions to the core,
and the state Cutter owns. They do not test rizin's own results.

## Layout

| Directory | Needs | Tests |
| --- | --- | --- |
| `unit/` | nothing | Cutter's own algorithms and pure classes |
| `session/` | core + fixture, no widgets | core operations emit the right signal, once, with the right data |
| `dialogs/` | app, no loaded file | dialog flows, including one dialog opening another: cancel, re-enter, error states; no MainWindow or docks |
| `workflows/` | full MainWindow + fixture | user scenarios across several widgets |
| `sweep/` | full MainWindow + fixture, Linux | every core signal against every connected slot (deadlocks); slow, aborts with every thread's stack |
| `common/` | — | shared helper; the only code that knows how the core is created |

Each `ctest` label matches its directory.

## Fixtures

`session/`, `dialogs/`, `workflows/` and `sweep/` load a few binaries from
[rizin-testbins](https://github.com/rizinorg/rizin-testbins). CMake downloads
only those files, at the commit pinned in `common/Fixtures.cmake`, into
`build/test-fixtures/` and checks each one's SHA-256. Nothing to set up.

If you already have rizin-testbins cloned, point CMake at it and nothing is
downloaded:

```sh
cmake -B build -DCUTTER_TESTBINS_DIR=/path/to/rizin-testbins
```

Offline, or with `-DCUTTER_FETCH_TEST_FIXTURES=OFF`, tests that need a missing
fixture are reported as skipped. To add a fixture, add a line to
`common/Fixtures.cmake`; `Fixtures.h` is generated from it.

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
