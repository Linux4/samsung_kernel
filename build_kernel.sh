#!/bin/bash
#export CROSS_COMPILE=../PLATFORM/prebuilts/gcc/linux-x86/arm/arm-eabi-4.8/bin/arm-eabi-
rm log.txt
make clean
make distclean
export CROSS_COMPILE=/opt/toolchains/arm-eabi-4.7/bin/arm-eabi-
export ARCH=arm

make exynos5430-base_defconfig
make -j24 2>&1 | tee -a  log.txt