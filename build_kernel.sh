#!/bin/bash

export ARCH=arm64
export PLATFORM_VERSION=10
export ANDROID_MAJOR_VERSION=q

make ARCH=arm64 exynos7885-a10e_defconfig
make ARCH=arm64 -j16