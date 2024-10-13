#!/bin/bash -e

# Copyright 2020 Google Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if [ -z $1 ]; then
    echo "usage: $0 <build number>"
    echo
    echo "The build number should be from a *snap* [not a buildkick] of aosp-build-tools-release."
    echo "https://android-build.googleplex.com/coastguard/branch/aosp-build-tools-release/#/request/"
    exit 1
fi

readonly BUILD_NUMBER=$1

cd "$(dirname $0)"

if ! git diff HEAD --quiet; then
    echo "must be run with a clean prebuilts/build-tools project"
    exit 1
fi

readonly tmpdir=$(mktemp -d)

function finish {
    if [ ! -z "${tmpdir}" ]; then
        rm -rf "${tmpdir}"
    fi
}
trap finish EXIT

function fetch_artifact() {
    /google/data/ro/projects/android/fetch_artifact --branch aosp-build-tools-release --bid ${BUILD_NUMBER} --target $1 "$2" "$3"
}

fetch_artifact linux build-prebuilts.zip "${tmpdir}/linux.zip"
fetch_artifact linux build-common-prebuilts.zip "${tmpdir}/common.zip"
fetch_artifact linux manifest_${BUILD_NUMBER}.xml "${tmpdir}/manifest.xml"
fetch_artifact darwin_mac build-prebuilts.zip "${tmpdir}/darwin.zip"
fetch_artifact linux_musl build-prebuilts.zip "${tmpdir}/linux_musl.zip"
fetch_artifact linux_musl build-arm64-prebuilts.zip "${tmpdir}/linux_musl_arm64.zip"
fetch_artifact linux_musl musl-sysroot-i686-unknown-linux-musl.zip "${tmpdir}/musl-sysroot-i686-unknown-linux-musl.zip"
fetch_artifact linux_musl musl-sysroot-x86_64-unknown-linux-musl.zip "${tmpdir}/musl-sysroot-x86_64-unknown-linux-musl.zip"
fetch_artifact linux_musl musl-sysroot-aarch64-unknown-linux-musl.zip "${tmpdir}/musl-sysroot-aarch64-unknown-linux-musl.zip"
fetch_artifact linux_musl musl-sysroot-arm-unknown-linux-musleabihf.zip "${tmpdir}/musl-sysroot-arm-unknown-linux-musleabihf.zip"

function unzip_to() {
    rm -rf "$1"
    mkdir "$1"
    unzip -q -DD -d "$1" "$2"
}

unzip_to linux-x86 "${tmpdir}/linux.zip"
unzip_to common "${tmpdir}/common.zip"
unzip_to darwin-x86 "${tmpdir}/darwin.zip"
unzip_to linux_musl-x86 "${tmpdir}/linux_musl.zip"
unzip_to linux_musl-arm64 "${tmpdir}/linux_musl_arm64.zip"
unzip_to sysroots/x86_64-unknown-linux-musl "${tmpdir}/musl-sysroot-x86_64-unknown-linux-musl.zip"
unzip_to sysroots/i686-unknown-linux-musl "${tmpdir}/musl-sysroot-i686-unknown-linux-musl.zip"
unzip_to sysroots/aarch64-unknown-linux-musl "${tmpdir}/musl-sysroot-aarch64-unknown-linux-musl.zip"
unzip_to sysroots/arm-unknown-linux-musleabihf "${tmpdir}/musl-sysroot-arm-unknown-linux-musleabihf.zip"

cp -f "${tmpdir}/manifest.xml" manifest.xml

git add manifest.xml linux-x86 linux_musl-arm64 linux_musl-x86 darwin-x86 common sysroots/x86_64-unknown-linux-musl sysroots/i686-unknown-linux-musl sysroots/aarch64-unknown-linux-musl sysroots/arm-unknown-linux-musleabihf
git commit -m "Update build-tools to ab/${BUILD_NUMBER}

https://ci.android.com/builds/branches/aosp-build-tools-release/grid?head=${BUILD_NUMBER}&tail=${BUILD_NUMBER}

Test: treehugger"
