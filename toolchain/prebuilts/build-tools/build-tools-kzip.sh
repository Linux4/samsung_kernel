#!/bin/bash -eu

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

# Builds cumulative cross-reference extraction file (.kzip) for all the 
# modules defined and buildable in this repository.
# Usage:
#   cd <repository top> && OUT_DIR=<output dir> prebuilts/build-tools-kzip.sh
# Generates $OUT_DIR/all.zkip

declare -r corpus=android.googlesource.com/platform/superproject
mkdir -p "${OUT_DIR:?Specify output directory}/soong"

# Build for x86, ignore missing references.
printf '{"Allow_missing_dependencies": true, "HostArch":"x86_64" }\n' >"${OUT_DIR}/soong/soong.variables"

# Some of targets will fail because of the missing references, but there
# will be sufficient number of successfully compiled modules.
XREF_CORPUS="${corpus}" build/soong/soong_ui.bash --make-mode --skip-make -k xref_java xref_cxx || echo Ignoring errors

# Package it all into a single .kzip, ignoring duplicates.
allkzip=$(realpath "${OUT_DIR}/all.kzip")
rm -f "${allkzip}"
tempdir=$(mktemp -d)
for zip in $(find "${OUT_DIR}" -name '*.kzip'); do
    unzip -qn "${zip}" -d "${tempdir}"
done
(cd "${tempdir}" && zip -9rmq "${allkzip}" . && rmdir "${tempdir}")
printf "Created %s with %d compilation units\n" "${allkzip}" $(zipinfo "${allkzip}" 'root/units/?*' | wc -l)

