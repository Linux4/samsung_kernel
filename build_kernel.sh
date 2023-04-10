#!/bin/bash

export ARCH=arm64
export CROSS_COMPILE=$(pwd)/../PLATFORM/prebuilts/gcc/linux-x86/aarch64/aarch64-linux-android-4.9/bin/aarch64-linux-android-

export ANDROID_MAJOR_VERSION=n
make ARCH=arm64 exynos7420-noblelte_defconfig
make ARCH=arm64