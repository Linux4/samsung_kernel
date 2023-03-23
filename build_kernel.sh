#!/bin/bash

export ARCH=arm64
export ANDROID_MAJOR_VERSION=q

make exynos9810-haechiy19lteatt_defconfig
make -j64

