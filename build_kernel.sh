#!/bin/bash

export ARCH=arm64
mkdir out

make -C $(pwd) O=$(pwd)/out DTC_EXT=$(pwd)/tools/dtc CONFIG_BUILD_ARM64_DT_OVERLAY=y CLANG_TRIPLE=aarch64-linux-gnu- vendor/f2q_usa_singlew_defconfig
make -C $(pwd) O=$(pwd)/out DTC_EXT=$(pwd)/tools/dtc CONFIG_BUILD_ARM64_DT_OVERLAY=y CLANG_TRIPLE=aarch64-linux-gnu-
 
cp out/arch/arm64/boot/Image $(pwd)/arch/arm64/boot/Image



