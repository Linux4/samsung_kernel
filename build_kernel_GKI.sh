#!/bin/bash

#1. target config
BUILD_TARGET=$1
export MODEL=$(echo $BUILD_TARGET | cut -d'_' -f1)
export PROJECT_NAME=${MODEL}
export REGION=$(echo $BUILD_TARGET | cut -d'_' -f2)
export CARRIER=$(echo $BUILD_TARGET | cut -d'_' -f3)
export TARGET_BUILD_VARIANT=$2
		
		
#2. sm8650 common config
CHIPSET_NAME=$3

export ANDROID_BUILD_TOP=$(pwd)
export TARGET_PRODUCT=gki
export TARGET_BOARD_PLATFORM=gki

export ANDROID_PRODUCT_OUT=${ANDROID_BUILD_TOP}/out/target/product/${MODEL}
export OUT_DIR=${ANDROID_BUILD_TOP}/out/msm-${CHIPSET_NAME}-${CHIPSET_NAME}-${TARGET_PRODUCT}


export KBUILD_EXT_MODULES="\
        ../vendor/qcom/opensource/display-drivers/msm \
        ../vendor/qcom/opensource/camera-kernel \
        ../vendor/qcom/opensource/audio-kernel \
        ../vendor/qcom/opensource/dsp-kernel \
        "

export  KBUILD_DISPLAY_EXTRA_SYMBOLS="\
          ../vendor/qcom/opensource/mmrm-driver/Module.symvers \
          ../vendor/qcom/opensource/mm-drivers/sync_fence/Module.symvers \
          ../vendor/qcom/opensource/mm-drivers/msm_ext_display/Module.symvers \
          ../vendor/qcom/opensource/mm-drivers/hw_fence/Module.symvers \
          ../vendor/qcom/opensource/securemsm-kernel/Module.symvers \
          "

#3. build kernel
cd ./kernel_platform/
RECOMPILE_KERNEL=1 ./build/android/prepare_vendor.sh ${CHIPSET_NAME} gki | tee -a ../build.log
