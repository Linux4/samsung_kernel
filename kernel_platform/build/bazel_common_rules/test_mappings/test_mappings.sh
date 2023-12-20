#!/bin/bash -e

# Copyright (C) 2022 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Ensure hermeticity.
PATH="$PWD/prebuilts/build-tools/path/linux-x86/:$PWD/prebuilts/build-tools/linux-x86/bin/"

while [[ $# -gt 0 ]]; do
  case $1 in
    --dist_dir)
      DIST_DIR="$2"
      shift # past argument
      shift # past value
      ;;
    --dist_dir=*)
      DIST_DIR=$1
      DIST_DIR="${DIST_DIR#*=}"
      shift # past argument=value
      ;;
    -*|--*)
      # There may be additional arguments passed to copy_to_dist_dir. Ignore them.
      shift
      ;;
    *)
      # There may be additional arguments passed to copy_to_dist_dir. Ignore them.
      shift
      ;;
  esac
done

# BUILD_WORKSPACE_DIRECTORY is the root of the Bazel workspace containing
# this binary target.
# https://docs.bazel.build/versions/main/user-manual.html#run
ROOT_DIR=$BUILD_WORKSPACE_DIRECTORY
if [[ -z "$ROOT_DIR" ]]; then
  echo "ERROR: Only execute this script with bazel run." >&2
  exit 1
fi

if [[ -z "$DIST_DIR" ]]; then
  echo "ERROR: --dist_dir is not specified." >&2
  exit 1
fi

if [[ ! "$DIST_DIR" == /* ]]; then
  DIST_DIR=${ROOT_DIR}/${DIST_DIR}
fi
mkdir -p ${DIST_DIR}

OUTPUT_FILE=${DIST_DIR}/test_mappings.zip
echo "Generating ${OUTPUT_FILE}"

trap 'rm -f "$TMPFILE"' EXIT
TEST_MAPPING_FILES=$(mktemp)
find ${ROOT_DIR} -name TEST_MAPPING \
  -not -path "${ROOT_DIR}/\.git*" \
  -not -path "${ROOT_DIR}/\.repo*" \
  -not -path "${ROOT_DIR}/out*" \
  > ${TEST_MAPPING_FILES}
soong_zip -o ${OUTPUT_FILE} -C ${ROOT_DIR} -l ${TEST_MAPPING_FILES}
rm -f ${TEST_MAPPING_FILES}
