#!/bin/bash

set -e

cd $(dirname "${BASH_SOURCE[0]}")/..
mkdir -p cutter-deps && cd cutter-deps

DEPS_BASE_URL=https://github.com/premades/cutter-deps/releases/download/v2

if [ "$CUTTER_QT" == "5" ]; then
	DEPS_FILE_linux_x86_64=cutter-deps-q5-linux-x86_64.tar.gz
	DEPS_SHA256_linux_x86_64=ab3099fe699db100f2d00e1b70cdf77dec6b8fdd9cd1709c96e123a15fb62571
	DEPS_BASE_URL=https://github.com/rizinorg/cutter-deps/releases/download/qt5-v17
else
	DEPS_FILE_linux_x86_64=cutter-deps-linux-x86_64.tar.gz
	DEPS_SHA256_linux_x86_64=019b12730b55d57c8352b1b2603ce009b3c837cb78d1f7594d557ce26815fa2e
fi
echo $DEPS_SHA256_linux_x86_64

DEPS_FILE_macos_x86_64=cutter-deps-macos-x86_64.tar.gz
DEPS_SHA256_macos_x86_64=4713ca125fdb56ff788aa50b94abf12af1342fd8bb5851c1f79a905431144699

DEPS_FILE_macos_arm64=cutter-deps-macos-arm64.tar.gz
DEPS_SHA256_macos_arm64=ee7ae89c06c190464cd84386fe1683147cf9d93a2bab0113a0250aa5d7f124f7

DEPS_FILE_win_x86_64=cutter-deps-win-x86_64.tar.gz
DEPS_SHA256_win_x86_64=0dbd73f5f83f7bc40aef6dd3d5254d7a7151d07abdaf08d84150c1ec37b520e2


ARCH=x86_64
if [ "$OS" == "Windows_NT" ]; then
	PLATFORM=win
else
	UNAME_S="$(uname -s)"
	if [ "$UNAME_S" == "Linux" ]; then
		PLATFORM=linux
	elif [ "$UNAME_S" == "Darwin" ]; then
		PLATFORM=macos
		ARCH=$(uname -m)
	else
		echo "Unsupported Platform: uname -s => $UNAME_S, \$OS => $OS"
		exit 1
	fi
fi

DEPS_FILE=DEPS_FILE_${PLATFORM}_${ARCH}
DEPS_FILE=${!DEPS_FILE}
DEPS_SHA256=DEPS_SHA256_${PLATFORM}_${ARCH}
DEPS_SHA256=${!DEPS_SHA256}
DEPS_URL=${DEPS_BASE_URL}/${DEPS_FILE}

SHA256SUM=sha256sum
if ! command -v ${SHA256SUM} &> /dev/null; then
	SHA256SUM="shasum -a 256"
fi

curl -L "$DEPS_URL" -o "$DEPS_FILE" || exit 1
echo "$DEPS_SHA256  $DEPS_FILE" | ${SHA256SUM} -c - || exit 1

tar -xf "$DEPS_FILE" || exit 1

if [ -f relocate.sh ]; then
	./relocate.sh || exit 1
fi

