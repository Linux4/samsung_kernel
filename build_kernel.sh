#!/bin/bash

export ARCH=arm64
export PLATFORM_VERSION=13

make ARCH=arm64 exynos850-a13xx_defconfig
make ARCH=arm64 -j16
