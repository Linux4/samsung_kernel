# SPDX-License-Identifier: GPL-2.0
#!/bin/bash

set -e

DEVICE_MODULES_DIR=$(basename $(dirname $0))
source "${DEVICE_MODULES_DIR}/kernel/kleaf/_setup_env.sh"

KLEAF_OUT=("--output_user_root=${OUT_DIR} --output_base=${OUT_DIR}/bazel/output_user_root/output_base")
KLEAF_ARGS=("${DEBUG_ARGS} ${SANDBOX_ARGS} --experimental_writable_outputs")

set -x
(
  export ${BAZEL_EXPORT_ENV} && \
	tools/bazel ${KLEAF_OUT} run ${KLEAF_ARGS} \
	--//build/bazel_mgk_rules:kernel_version=${KERNEL_VERSION_NUM} \
	//${DEVICE_MODULES_DIR}:${PROJECT}.user_abi_update_symbol_list
  tools/bazel ${KLEAF_OUT} run ${KLEAF_ARGS} //${KERNEL_VERSION}:kernel_aarch64_abi_update
)

