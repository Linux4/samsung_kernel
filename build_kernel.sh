#!/bin/bash

export ARCH=arm64
export PLATFORM_VERSION=11
export ANDROID_MAJOR_VERSION=r

make ARCH=arm64 exynos2100-o1sxxx_defconfig
make ARCH=arm64 -j16