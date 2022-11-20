#!/bin/bash
##
#  Copyright (C) 2015, Samsung Electronics, Co., Ltd.
#  Written by System S/W Group, S/W Platform R&D Team,
#  Mobile Communication Division.
##

set -e -o pipefail

export CROSS_C=../PLATFORM/prebuilts/gcc/linux-x86/arm/arm-eabi-4.7/bin/arm-eabi-
export ARCH=arm

PLATFORM=sc8830
DEFCONFIG=goyave3g_defconfig

KERNEL_PATH=$(pwd)
MODULE_PATH=${KERNEL_PATH}/modules
EXTERNAL_MODULE_PATH=${KERNEL_PATH}/external_module

JOBS=`grep processor /proc/cpuinfo | wc -l`

function build_kernel() {
	make CROSS_COMPILE=$CROSS_C ${DEFCONFIG}
	make CROSS_COMPILE=$CROSS_C -j${JOBS}
	make CROSS_COMPILE=$CROSS_C modules
	make CROSS_COMPILE=$CROSS_C dtbs
	make CROSS_COMPILE=$CROSS_C -C ${EXTERNAL_MODULE_PATH}/mali MALI_PLATFORM=${PLATFORM} BUILD=release KDIR=${KERNEL_PATH}

	[ -d ${MODULE_PATH} ] && rm -rf ${MODULE_PATH}
	mkdir -p ${MODULE_PATH}

	find ${KERNEL_PATH}/drivers -name "*.ko" -exec cp -f {} ${MODULE_PATH} \;
	find ${EXTERNAL_MODULE_PATH} -name "*.ko" -exec cp -f {} ${MODULE_PATH} \;
}

function clean() {
	[ -d ${MODULE_PATH} ] && rm -rf ${MODULE_PATH}
	make CROSS_COMPILE=$CROSS_C distclean
}

function main() {
	[ "${1}" = "Clean" ] && clean || build_kernel
}

main $@
