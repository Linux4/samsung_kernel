#!/bin/bash

export PLATFORM_VERSION=12
export ANDROID_MAJOR_VERSION=s
export ARCH=arm64
make exynos980-a51x_defconfig
make

cp out/arch/arm64/boot/Image $(pwd)/arch/arm64/boot/Image
