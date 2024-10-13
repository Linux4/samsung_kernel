#!/bin/bash

#1. target config
BUILD_TARGET=m55xq_swa_open
export MODEL=$(echo $BUILD_TARGET | cut -d'_' -f1)
export PROJECT_NAME=${MODEL}
export REGION=$(echo $BUILD_TARGET | cut -d'_' -f2)
export CARRIER=$(echo $BUILD_TARGET | cut -d'_' -f3)
export TARGET_BUILD_VARIANT=user
		
		
#2. sm8650 common config
CHIPSET_NAME=x55

export ANDROID_BUILD_TOP=$(pwd)
export TARGET_PRODUCT=gki
export TARGET_BOARD_PLATFORM=gki

export ANDROID_PRODUCT_OUT=${ANDROID_BUILD_TOP}/out/target/product/${MODEL}
export OUT_DIR=${ANDROID_BUILD_TOP}/out/msm-${CHIPSET_NAME}-${CHIPSET_NAME}-${TARGET_PRODUCT}

export IS_KBUILD=true

#3. build kernel
cd ./kernel_platform/
RECOMPILE_KERNEL=1 ./build/android/prepare_vendor.sh ${CHIPSET_NAME} ${TARGET_PRODUCT} gki | tee -a ../build.log
