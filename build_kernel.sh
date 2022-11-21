#!/bin/bash

export ARCH=arm64
export PLATFORM_VERSION=9
export ANDROID_MAJOR_VERSION=p
export CONFIG_SECTION_MISMATCH_WARN_ONLY=y

make ARCH=arm64 exynos7570-xcover4lte_defconfig
make ARCH=arm64 -j64