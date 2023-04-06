#!/bin/bash -ex

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

: "${OUT_DIR:?Must set OUT_DIR}"
TOP=$(pwd)

UNAME="$(uname)"
case "$UNAME" in
Linux)
    OS='linux'
    ;;
Darwin)
    OS='darwin'
    ;;
*)
    exit 1
    ;;
esac

build_soong=1
[[ ! -d ${TOP}/toolchain/go ]] || build_go=1
clean=t
[[ "${1:-}" != '--resume' ]] || clean=''

# Use toybox and other prebuilts even outside of the build (test running, go, etc)
export PATH=${TOP}/prebuilts/build-tools/path/${OS}-x86:$PATH

if [ -n ${build_soong} ]; then
    SOONG_OUT=${OUT_DIR}/soong
    SOONG_HOST_OUT=${OUT_DIR}/soong/host/${OS}-x86
    [[ -z "${clean}" ]] || rm -rf ${SOONG_OUT}
    mkdir -p ${SOONG_OUT}
    rm -rf ${SOONG_OUT}/dist ${SOONG_OUT}/dist-common
    cat > ${SOONG_OUT}/soong.variables << EOF
{
    "Allow_missing_dependencies": true,
    "HostArch":"x86_64",
    "VendorVars": {
        "cpython3": {
            "force_build_host": "true"
        },
        "art_module": {
            "source_build": "true"
        }
    }
}
EOF
    SOONG_BINARIES=(
        acp
        aidl
        bison
        bloaty
        bpfmt
        bzip2
        ckati
        ckati_stamp_dump
        flex
        gavinhoward-bc
        go_extractor
        hidl-gen
        hidl-lint
        m4
        make
        ninja
        one-true-awk
        openssl
        py2-cmd
        py3-cmd
        py3-launcher64
        py3-launcher-autorun64
        runextractor
        soong_zip
        toybox
        xz
        zip2zip
        zipalign
        ziptime
        ziptool
    )
    SOONG_ASAN_BINARIES=(
        acp
        aidl
        ckati
        gavinhoward-bc
        ninja
        toybox
        zipalign
        ziptime
        ziptool
    )
    SOONG_JAVA_LIBRARIES=(
        desugar.jar
        dx.jar
        turbine.jar
        javac_extractor.jar
    )
    SOONG_JAVA_WRAPPERS=(
        dx
    )
    if [[ $OS == "linux" ]]; then
        SOONG_BINARIES+=(
            create_minidebuginfo
            nsjail
        )
    fi

    binaries="${SOONG_BINARIES[@]/#/${SOONG_HOST_OUT}/bin/}"
    asan_binaries="${SOONG_ASAN_BINARIES[@]/#/${SOONG_HOST_OUT}/bin/}"
    jars="${SOONG_JAVA_LIBRARIES[@]/#/${SOONG_HOST_OUT}/framework/}"
    wrappers="${SOONG_JAVA_WRAPPERS[@]/#/${SOONG_HOST_OUT}/bin/}"

    # TODO: When we have a better method of extracting zips from Soong, use that.
    py3_stdlib_zip="${SOONG_OUT}/.intermediates/external/python/cpython3/Lib/py3-stdlib-zip/gen/py3-stdlib.zip"

    # Build everything
    build/soong/soong_ui.bash --make-mode --skip-make \
        ${binaries} \
        ${wrappers} \
        ${jars} \
        ${py3_stdlib_zip} \
        ${SOONG_HOST_OUT}/nativetest64/ninja_test/ninja_test \
        ${SOONG_HOST_OUT}/nativetest64/ckati_test/find_test \
        soong_docs

    # Run ninja tests
    ${SOONG_HOST_OUT}/nativetest64/ninja_test/ninja_test

    # Run ckati tests
    ${SOONG_HOST_OUT}/nativetest64/ckati_test/find_test

    # Copy arch-specific binaries
    mkdir -p ${SOONG_OUT}/dist/bin
    cp ${binaries} ${SOONG_OUT}/dist/bin/
    cp -R ${SOONG_HOST_OUT}/lib* ${SOONG_OUT}/dist/

    # Copy jars and wrappers
    mkdir -p ${SOONG_OUT}/dist-common/{bin,flex,framework,py3-stdlib}
    cp ${wrappers} ${SOONG_OUT}/dist-common/bin
    cp ${jars} ${SOONG_OUT}/dist-common/framework

    cp -r external/bison/data ${SOONG_OUT}/dist-common/bison
    cp external/bison/NOTICE ${SOONG_OUT}/dist-common/bison/
    cp -r external/flex/src/FlexLexer.h ${SOONG_OUT}/dist-common/flex/
    cp external/flex/NOTICE ${SOONG_OUT}/dist-common/flex/

    unzip -q -d ${SOONG_OUT}/dist-common/py3-stdlib ${py3_stdlib_zip}
    cp external/python/cpython3/LICENSE ${SOONG_OUT}/dist-common/py3-stdlib/

    if [[ $OS == "linux" ]]; then
        # Build ASAN versions
        export ASAN_OPTIONS=detect_leaks=0
        cat > ${SOONG_OUT}/soong.variables << EOF
{
    "Allow_missing_dependencies": true,
    "HostArch":"x86_64",
    "SanitizeHost": ["address"],
    "VendorVars": {
        "art_module": {
            "source_build": "true"
        }
    }
}
EOF

        export ASAN_SYMBOLIZER_PATH=${PWD}/prebuilts/clang/host/linux-x86/llvm-binutils-stable/llvm-symbolizer

        # Clean up non-ASAN installed versions
        rm -rf ${SOONG_HOST_OUT}

        # Build everything with ASAN
        build/soong/soong_ui.bash --make-mode --skip-make \
            ${asan_binaries} \
            ${SOONG_HOST_OUT}/nativetest64/ninja_test/ninja_test \
            ${SOONG_HOST_OUT}/nativetest64/ckati_test/find_test

        # Run ninja tests
        ${SOONG_HOST_OUT}/nativetest64/ninja_test/ninja_test

        # Run ckati tests
        ${SOONG_HOST_OUT}/nativetest64/ckati_test/find_test

        # Copy arch-specific binaries
        mkdir -p ${SOONG_OUT}/dist/asan/bin
        cp ${asan_binaries} ${SOONG_OUT}/dist/asan/bin/
        cp -R ${SOONG_HOST_OUT}/lib* ${SOONG_OUT}/dist/asan/
    fi

    # Package arch-specific prebuilts
    (
        cd ${SOONG_OUT}/dist
        zip -qryX build-prebuilts.zip *
    )

    # Package common prebuilts
    (
        cd ${SOONG_OUT}/dist-common
        zip -qryX build-common-prebuilts.zip *
    )
fi

# Go
if [ -n ${build_go} ]; then
    GO_OUT=${OUT_DIR}/obj/go
    rm -rf ${GO_OUT}
    mkdir -p ${GO_OUT}
    cp -a ${TOP}/toolchain/go/* ${GO_OUT}/
    (
        cd ${GO_OUT}/src
        export GOROOT_BOOTSTRAP=${TOP}/prebuilts/go/${OS}-x86
        export GOROOT_FINAL=./prebuilts/go/${OS}-x86
        export GO_TEST_TIMEOUT_SCALE=100
        ./make.bash
        rm -rf ../pkg/bootstrap
        rm -rf ../pkg/obj
        GOROOT=$(pwd)/.. ../bin/go install -race std
    )
    (
        cd ${GO_OUT}
        zip -qryX go.zip * --exclude update_prebuilts.sh
    )
fi

if [ -n "${DIST_DIR}" ]; then
    mkdir -p ${DIST_DIR} || true

    if [ -n ${build_soong} ]; then
        cp ${SOONG_OUT}/dist/build-prebuilts.zip ${DIST_DIR}/
        cp ${SOONG_OUT}/dist-common/build-common-prebuilts.zip ${DIST_DIR}/
        cp ${SOONG_OUT}/docs/*.html ${DIST_DIR}/
    fi
    if [ -n ${build_go} ]; then
        cp ${GO_OUT}/go.zip ${DIST_DIR}/
    fi
fi
