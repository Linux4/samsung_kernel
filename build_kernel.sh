#!/bin/bash

export ARCH=arm64
mkdir out

export ANDROID_MAJOR_VERSION=q

make ARCH=arm64 exynos9810-crownlteks_defconfig

make ARCH=arm64 -j16

cp out/arch/arm64/boot/Image $(pwd)/arch/arm64/boot/Image
