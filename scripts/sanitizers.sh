#!/usr/bin/env bash
# Builds Cutter and runs its tests under a sanitizer, for .github/workflows/sanitizers.yml.
#
#   sanitizers.sh deps
#   sanitizers.sh build <source dir> <build dir> asan|lsan
#   sanitizers.sh test <build dir> asan|lsan <log dir>
#
# Only Cutter is instrumented. Rizin gets frame pointers and debug info, so stacks through it are
# complete, but no sanitizer: its own memory bugs are not this job's concern.
set -euo pipefail

usage() {
    sed -n '4,6p' "$0" >&2
    exit 2
}

sanitizer_flags() {
    case "$1" in
    asan) echo "-fsanitize=address,undefined -fno-sanitize-recover=undefined" ;;
    lsan) echo "-fsanitize=leak" ;;
    *) usage ;;
    esac
}

command=${1:-}
[ $# -gt 0 ] && shift

case "$command" in
deps)
    sudo apt-get update
    sudo apt-get install -y --no-install-recommends \
        ninja-build ccache pkg-config gcc-12 g++-12 llvm-12 \
        qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools \
        libqt6svg6-dev libqt6svgwidgets6 libqt6core5compat6-dev libqt6opengl6-dev \
        libgl1-mesa-dev mesa-common-dev libxkbcommon-dev libxkbcommon-x11-dev libpcre2-dev
    python3 -m pip install --user meson
    mkdir -p "$HOME/.local/bin"
    ln -sf /usr/bin/llvm-symbolizer-12 "$HOME/.local/bin/llvm-symbolizer"
    echo "$HOME/.local/bin" >>"$GITHUB_PATH"
    ;;

build)
    [ $# -eq 3 ] || usage
    source_dir=$1
    build_dir=$2
    sanitizer=$(sanitizer_flags "$3")

    export CC=gcc-12 CXX=g++-12
    # The bundled Rizin is built by meson, which takes its flags from the environment.
    export CFLAGS="-g -fno-omit-frame-pointer" CXXFLAGS="-g -fno-omit-frame-pointer"

    cmake -S "$source_dir" -B "$build_dir" -G Ninja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_C_FLAGS_RELWITHDEBINFO="-O1 -g" \
        -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O1 -g" \
        -DCMAKE_C_FLAGS="$CFLAGS $sanitizer" \
        -DCMAKE_CXX_FLAGS="$CXXFLAGS $sanitizer" \
        -DCMAKE_EXE_LINKER_FLAGS="$sanitizer" \
        -DCMAKE_SHARED_LINKER_FLAGS="$sanitizer" \
        -DCMAKE_C_COMPILER_LAUNCHER=ccache \
        -DCMAKE_CXX_COMPILER_LAUNCHER=ccache \
        -DCUTTER_QT=6 \
        -DCUTTER_USE_BUNDLED_RIZIN=ON \
        -DCUTTER_ENABLE_TEST=ON \
        -DCUTTER_ENABLE_PYTHON=OFF \
        -DCUTTER_ENABLE_GRAPHVIZ=OFF \
        -DCUTTER_ENABLE_KSYNTAXHIGHLIGHTING=OFF \
        -DCUTTER_ENABLE_TRANSLATIONS=OFF
    cmake --build "$build_dir"
    ;;

test)
    [ $# -eq 3 ] || usage
    build_dir=$1
    sanitizer_flags "$2" >/dev/null
    mkdir -p "$3"
    logs=$(realpath "$3")
    common="log_exe_name=1:external_symbolizer_path=$(command -v llvm-symbolizer || true)"

    case "$2" in
    asan)
        export ASAN_OPTIONS="detect_leaks=0:halt_on_error=1:log_path=$logs/asan:$common"
        export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1:log_path=$logs/ubsan:$common"
        ;;
    lsan)
        # Full stacks through the uninstrumented Rizin need the slow unwinder.
        export LSAN_OPTIONS="exitcode=0:fast_unwind_on_malloc=0:malloc_context_size=40:log_path=$logs/lsan:$common"
        ;;
    esac

    # The test results don't decide the job, the sanitizer logs do: some tests are red on purpose
    # until the bug they describe is fixed.
    ctest --test-dir "$build_dir" --output-on-failure -j"$(nproc)" || true
    ;;

*)
    usage
    ;;
esac
