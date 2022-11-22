#!/bin/bash
export CROSS_COMPILE=../PLATFORM/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-

#export CROSS_COMPILE=/opt/toolchains/arm-eabi-4.9/bin/aarch64-linux-android-
export ARCH=arm64

make exynos7580-a5xelte_defconfig
make -j
#24 2>&1 | tee -a  log.txt