#!/bin/bash -e

if [ -z $1 ]; then
    echo "usage: $0 <build number>"
    exit 1
fi

readonly BUILD_NUMBER=$1

cd "$(dirname $0)"

if ! git diff HEAD --quiet; then
    echo "must be run with a clean prebuilts/kernel-build-tools project"
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
    /google/data/ro/projects/android/fetch_artifact --branch aosp_kernel-build-tools --bid ${BUILD_NUMBER} --target $1 "$2" "$3"
}

fetch_artifact linux build-prebuilts.zip "${tmpdir}/linux.zip"
fetch_artifact linux manifest_${BUILD_NUMBER}.xml "${tmpdir}/manifest.xml"

function unzip_to() {
    rm -rf "$1"
    mkdir "$1"
    unzip -q -d "$1" "$2"
}

unzip_to linux-x86 "${tmpdir}/linux.zip"

cp -f "${tmpdir}/manifest.xml" manifest.xml

git add manifest.xml linux-x86
git commit -m "Update kernel-build-tools to ab/${BUILD_NUMBER}

https://ci.android.com/builds/branches/aosp_kernel-build-tools/grid?head=${BUILD_NUMBER}&tail=${BUILD_NUMBER}

Test: treehugger"
