@echo off
rem Builds Cutter and its tests on Windows, for .github/workflows/tests.yml.
rem Run from the repository root after scripts/fetch_deps.sh.
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64 || exit /b 1
set PATH=%CD%\cutter-deps\qt\bin;%PATH%

cmake -S . -B build -G Ninja ^
    -DCMAKE_BUILD_TYPE=RelWithDebInfo ^
    -DCMAKE_PREFIX_PATH="%CD%\cutter-deps\qt" ^
    -DCMAKE_TOOLCHAIN_FILE=%VCPKG_INSTALLATION_ROOT%/scripts/buildsystems/vcpkg.cmake ^
    -DCUTTER_USE_BUNDLED_RIZIN=ON ^
    -DCUTTER_ENABLE_TEST=ON ^
    -DCUTTER_ENABLE_PYTHON=OFF ^
    -DCUTTER_ENABLE_GRAPHVIZ=OFF ^
    -DCUTTER_ENABLE_KSYNTAXHIGHLIGHTING=OFF ^
    -DCUTTER_ENABLE_TRANSLATIONS=OFF || exit /b 1
cmake --build build || exit /b 1

rem The tests find the Qt and Rizin DLLs through PATH.
echo %CD%\cutter-deps\qt\bin>>"%GITHUB_PATH%"
echo %CD%\build>>"%GITHUB_PATH%"
