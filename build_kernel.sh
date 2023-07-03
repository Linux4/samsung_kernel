#!/bin/bash

export ARCH=arm64
export PLATFORM_VERSION=8.1
export ANDROID_MAJOR_VERSION=o

make ARCH=arm64 exynos7570-j3popelteatt_defconfig
make ARCH=arm64 -j16
