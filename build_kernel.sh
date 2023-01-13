#!/bin/bash


export PLATFORM_VERSION=13
export ANDROID_MAJOR_VERSION=T
export ARCH=arm64

make ARCH=arm64 exynos850-a13sq_defconfig
make ARCH=arm64 -j64