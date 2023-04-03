#!/bin/bash

export SEC_BUILD_CONF_VENDOR_BUILD_OS=13
export PLATFORM_VERSION=11
export ANDROID_MAJOR_VERSION=r
export ARCH=arm64

make exynos9830-c1sxxx_defconfig
make -j16

