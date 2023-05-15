#!/bin/bash

export ARCH=arm64
export PLATFORM_VERSION=11
export ANDROID_MAJOR_VERSION=r
export CONFIG_SECTION_MISMATCH_WARN_ONLY=y

make ARCH=arm64 exynos9610-m30sdd_defconfig
make ARCH=arm64 -j16