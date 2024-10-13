#!/bin/bash

export ARCH=arm64
export PLATFORM_VERSION=14
export ANDROID_MAJOR_VERSION=u

make ARCH=arm64 exynos850-m13dd_defconfig
make ARCH=arm64 -j16
