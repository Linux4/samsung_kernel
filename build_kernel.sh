#!/bin/bash

export ARCH=arm64


export PLATFORM_VERSION=11
export ANDROID_MAJOR_VERSION=r
export ARCH=arm64

make ARCH=arm64 exynos9610-m30sdd_defconfig
make ARCH=arm64 -j64
