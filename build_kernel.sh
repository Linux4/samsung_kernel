#!/bin/bash

export ARCH=arm64


export PLATFORM_VERSION=10
export ANDROID_MAJOR_VERSION=q
export ARCH=arm64

make ARCH=arm64 exynos7885-a6eltetmo_defconfig
make ARCH=arm64 -j64

