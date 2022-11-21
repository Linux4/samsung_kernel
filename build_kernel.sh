#!/bin/bash

#export ARCH=arm64
#export CROSS_COMPILE=/tools/prebuilts/gcc-cfp-jopp-only/aarch64-linux-android-4.9/bin/aarch64-linux-android-

export ANDROID_MAJOR_VERSION=o
make ARCH=arm64 exynos7885-gview2lte_usa_att_defconfig
make ARCH=arm64
