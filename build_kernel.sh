#!/bin/bash

export ARCH=arm64
export PLATFORM_VERSION=13
export ANDROID_MAJOR_VERSION=t

make ARCH=arm64 exynos850-a04sxx_defconfig
make ARCH=arm64 -j16
