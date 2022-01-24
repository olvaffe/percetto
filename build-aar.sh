#!/usr/bin/env bash
# Copyright 2021-2022, Collabora, Ltd.
#
# SPDX-License-Identifier: BSL-1.0

set -e

# shellcheck disable=SC2086
ROOT=$(cd "$(dirname $0)" && pwd)
PROJECT=percetto
DEFAULT_NDK_VERSION=21.4.7075529

ANDROID_NDK_HOME=${ANDROID_NDK_HOME:-${HOME}/Android/Sdk/ndk/${DEFAULT_NDK_VERSION}}

if [ ! -f "${ANDROID_NDK_HOME}/build/cmake/android.toolchain.cmake" ]; then
    echo "Please set ANDROID_NDK_HOME to a valid, installed NDK!"
    exit 1
fi

export ANDROID_NDK_HOME

BUILD_DIR=${BUILD_DIR:-${ROOT}/build}
INSTALL_DIR=${INSTALL_DIR:-${ROOT}/install}
GENERATION_DIR="${BUILD_DIR}/Android_x86_aar/src"

rm -f "${GENERATION_DIR}/*.pom" || true

rm -rf "${INSTALL_DIR}"
for arch in x86 x86_64 armeabi-v7a arm64-v8a; do
    cmake --preset Android_${arch}_aar
    cmake --build --preset Android_${arch}_aar
done
# find latest pom
DECORATED=$(cd "${GENERATION_DIR}" && find . -maxdepth 1 -name "${PROJECT}*.pom" | sort | tail -n 1)
# strip leading ./
DECORATED=${DECORATED#./}
echo "DECORATED ${DECORATED}"
DECORATED_STEM=${DECORATED%.pom}
echo "DECORATED_STEM ${DECORATED_STEM}"
VERSION=${DECORATED_STEM#${PROJECT}-}
echo "VERSION ${VERSION}"
DIR=$(pwd)/maven_repo/io/github/olvaffe/deps/${PROJECT}/${VERSION}

mkdir -p "${DIR}"
cp "${GENERATION_DIR}/${DECORATED}" "${ROOT}"
cp "${GENERATION_DIR}/${DECORATED}" "${DIR}"

(
    cd "$INSTALL_DIR/${PROJECT}"
    7za a -r ../${PROJECT}.zip ./*
    mv ../${PROJECT}.zip "$ROOT/${DECORATED_STEM}.aar"
    cp "$ROOT/${DECORATED_STEM}.aar" "$DIR/${DECORATED_STEM}.aar"
)
