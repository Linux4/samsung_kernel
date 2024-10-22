# SPDX-License-Identifier: GPL-2.0
#!/bin/bash

export ROOT_DIR=$(readlink -f $PWD)
export BUILD_CONFIG=${BUILD_CONFIG:-build.config}
KERNEL_VERSION_NUM=${DEVICE_MODULES_DIR##kernel_device_modules-}
KERNEL_VERSION=kernel-${KERNEL_VERSION_NUM}

echo ROOT_DIR: ${ROOT_DIR}
echo DEVICE_MODULES_DIR: ${DEVICE_MODULES_DIR}
echo KERNEL_VERSION: ${KERNEL_VERSION}

set -a
. ${ROOT_DIR}/${BUILD_CONFIG}
for fragment in ${BUILD_CONFIG_FRAGMENTS}; do
  . ${ROOT_DIR}/${fragment}
done
set +a

if [ -z ${PROJECT} ]
then
  echo "ERROR: PROJECT must be set!"
  exit 1
fi
if [ -z ${MODE} ]
then
  MODE=user
fi

if [ -z ${OUT_DIR} ]
then
  OUT_DIR=${ROOT_DIR}/out
fi
if ! [ "x${OUT_DIR}" = "x/*" ]
then
  OUT_DIR=$(readlink -f ${ROOT_DIR}/${OUT_DIR})
fi

if [ "x${DEBUG}" == "x1" ]
then
DEBUG_ARGS="--sandbox_debug --verbose_failures"
fi
if [ -z ${SANDBOX} ]
then
  SANDBOX=1
else
  if [ "x${SANDBOX}" == "x0" ]
  then
    SANDBOX_ARGS="--config=local"
  fi
fi

BAZEL_EXPORT_ENV="BAZEL_DO_NOT_DETECT_CPP_TOOLCHAIN=1 \
	DEFCONFIG_OVERLAYS=${DEFCONFIG_OVERLAYS} \
	KERNEL_VERSION=${KERNEL_VERSION} \
	SOURCE_DATE_EPOCH=0"
