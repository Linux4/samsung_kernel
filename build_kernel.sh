#!/bin/bash

export ARCH=arm64
mkdir out
export ANDROID_MAJOR_VERSION=r
make a50_usa_open_defconfig
make -j64

cp out/arch/arm64/boot/Image $(pwd)/arch/arm64/boot/Image
