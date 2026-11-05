#!/usr/bin/env bash
# Builds Cutter and its tests on Linux and macOS, for .github/workflows/tests.yml.
# Run from the repository root after scripts/fetch_deps.sh.
source cutter-deps/env.sh
set -euo pipefail

cmake -S . -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER_LAUNCHER=ccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
    -DCUTTER_USE_BUNDLED_RIZIN=ON \
    -DCUTTER_ENABLE_TEST=ON \
    -DCUTTER_ENABLE_PYTHON=OFF \
    -DCUTTER_ENABLE_GRAPHVIZ=OFF \
    -DCUTTER_ENABLE_KSYNTAXHIGHLIGHTING=OFF \
    -DCUTTER_ENABLE_TRANSLATIONS=OFF
cmake --build build
