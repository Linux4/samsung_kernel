#!/bin/bash



export ANDROID_MAJOR_VERSION=p
export ARCH=arm64

make ARCH=arm64 exynos8895-dream2ltekor_defconfig
make ARCH=arm64 -j64


