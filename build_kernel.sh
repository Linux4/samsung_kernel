#!/bin/bash

export ARCH=arm64
#export CROSS_COMPILE=../PLATFORM/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-
export CROSS_COMPILE=scripts/toolchain/gcc-cfp/gcc-cfp-jopp-only/aarch64-linux-android-4.9/bin/aarch64-linux-android-

export ANDROID_MAJOR_VERSION=p
make exynos8895-greatltekor_defconfig
make
